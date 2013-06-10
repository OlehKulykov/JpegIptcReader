/* pyiptcdat.c -- functions describiing a Data object
 *
 * Copyright © 2007 Ian Wienand <ianw@ieee.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "pyiptcdata.h"
#include <errno.h>

/* use this to generally bail out if the file has been closed */
#define check_dataobject_open(data_obj)						\
if (data_obj->state == CLOSED) {						\
	PyErr_SetString(PyExc_ValueError, "operation on closed dataset");	\
	 return NULL;								\
}

#define TMP_TEMPLATE "pyiptcdata.XXXXXX"

/* allocate a new data object, with a blank list for dataset objects */
DataObject *
newDataObject(PyObject *arg)
{
	DataObject *self;
	self = PyObject_New(DataObject, &Data_Type);
	if (self == NULL)
		return NULL;
	self->DataSet_list = PyList_New(0);
	self->filename = NULL;
	self->state = CLOSED;
	if (self->DataSet_list == NULL)
		return NULL;
	return self;
}


static void
dealloc(DataObject *self)
{
	/* free our memory from the iptc library */
	iptc_data_unref(self->d);
	self->d = NULL;

	/* free ourself, as per Python tutorial */
	self->ob_type->tp_free(self);
}


/* Data methods */
static PyObject *
get_datasets(DataObject *self, void *closure)
{
	check_dataobject_open(self);
	Py_INCREF(self->DataSet_list);
	return self->DataSet_list;
}

static PyGetSetDef getseters[] = {
	{"datasets", (getter)get_datasets,
	 (setter)NULL, "Get the datasets associated with this data object", NULL},
	{NULL}
};

static PyObject *
add_dataset(DataObject *self, PyObject *args)
{
	IptcRecord record;
	IptcTag tag;
	IptcDataSet *ds;
	DataSetObject *dso;

	if (!PyArg_ParseTuple(args, "(ii)", &record, &tag))
		return NULL;

	check_dataobject_open(self);

	/* add the dataset via the IPTC library */
	ds = iptc_dataset_new();
	iptc_dataset_set_tag(ds, record, tag);
	iptc_data_add_dataset(self->d, ds);

	/* setup and append the new object to our list */
	dso = newDataSetObject(ds);
	dso->parent = self;
	/* When a dataset is removed, it decrements the reference
	 * count on its parent.  Thus when we add a dataset, we
	 * increase the parent reference count. */
	Py_INCREF(self);

	dso->state = VALID;

	PyList_Append(self->DataSet_list, (PyObject *)dso);

	return (PyObject*)dso;
}

static PyObject *
save(DataObject *self, PyObject *args, PyObject *keywds)
{
	unsigned char *iptc_buf = NULL;
	unsigned int iptc_len;

	unsigned char old_ps3[PS3_BUFLEN];
	unsigned char new_ps3[PS3_BUFLEN];
	unsigned int old_ps3_len, new_ps3_len;

	/* before we touch anything, make sure we have not been opened */
	check_dataobject_open(self);

	char *arg_filename = PyString_AsString(self->filename);

	static char *kwlist[] = {"filename", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, keywds, "|s", kwlist,
					&arg_filename))
		return NULL;

	/* build temporary filename template with same directory */
	int file_len = strlen(arg_filename);
	char * tmp_filename = calloc(1, sizeof(TMP_TEMPLATE) + file_len + 1);
	if (tmp_filename == NULL)
		return NULL;
	char * basename = strrchr(arg_filename, '/');
	if (basename) {
		int path_len = file_len - strlen(basename) + 1;
		strncpy(tmp_filename, arg_filename, path_len);
	}
	strcat(tmp_filename, TMP_TEMPLATE);

	FILE *infile, *outfile;
	int outfile_fd;

	/* open up the old file. */
	infile = fopen (arg_filename, "r");
	if (!infile) {
		free(tmp_filename);
		return PyErr_SetFromErrnoWithFilename(PyExc_IOError,
						PyString_AsString(self->filename));
	}

	/* create a new temporary output file */
	outfile_fd = mkstemp(tmp_filename);
	if (!outfile_fd) {
		fclose(infile);
		free(tmp_filename);
		return PyErr_SetFromErrno(PyExc_IOError);
	}

	/* open stream for temporary file */
	outfile = fdopen(outfile_fd, "wx");
	if (!outfile) {
		fclose(infile);
		free(tmp_filename);
		return PyErr_SetFromErrno(PyExc_IOError);
	}

	/* read in old PS3 data.  Other areas will therefore be
	 * retained */
	old_ps3_len = iptc_jpeg_read_ps3(infile, old_ps3, PS3_BUFLEN);
	if (old_ps3_len < 0) {
		free(tmp_filename);
		return NULL;
	}


	/* setup our iptc header */

	/* The following two lines can hurt Picasa compatability, so they
	 * are commented out. */
	//iptc_data_set_version (self->d, IPTC_IIM_VERSION);
	//iptc_data_set_encoding_utf8 (d);
	iptc_data_sort (self->d);

	/* save our IPTC data to a new stream */
	if (iptc_data_save(self->d, &iptc_buf, &iptc_len) < 0) {
		free(tmp_filename);
		return NULL;
	}

	/* now save that stream into a photoshop header */
	new_ps3_len = iptc_jpeg_ps3_save_iptc(old_ps3, old_ps3_len,
					iptc_buf, iptc_len, new_ps3, PS3_BUFLEN);

	/* free up the data stream */
	iptc_data_free_buf(self->d, iptc_buf);

	/* now save this header into the actual jpeg. */
	rewind(infile);
	if (iptc_jpeg_save_with_ps3 (infile, outfile, new_ps3, new_ps3_len) < 0) {
		free(tmp_filename);
		fprintf (stderr, "Failed to save image\n");
		return NULL;
	}

	fclose (infile);
	fclose (outfile);

	/* rename to the new image */
	if (rename (tmp_filename, arg_filename) < 0) {
		free(tmp_filename);
		return PyErr_SetFromErrnoWithFilename(PyExc_IOError,
						arg_filename);
	}

	free(tmp_filename);
	Py_RETURN_NONE;
}


static PyObject *
close_it(DataObject *self, PyObject *args) {

	int i;

	/* check this is actually open */
	check_dataobject_open(self)

	Py_CLEAR(self->filename);

	for(i=0 ; i < PyList_GET_SIZE(self->DataSet_list) ; i++)
	{
		DataSetObject *dso = (DataSetObject *)PyList_GetItem(self->DataSet_list, i);
		/* We clear our reference on the dataset list, but
		 * they still have a reference back to us.  Only once
		 * all these object have been done with will this
		 * object be de-allocated. */
		Py_CLEAR(dso);
	}
	Py_CLEAR(self->DataSet_list);

	self->state = CLOSED;

	Py_RETURN_NONE;
}

static PyMethodDef methods[] = {
	{"save",	(PyCFunction)save,	 METH_VARARGS|METH_KEYWORDS,
	 PyDoc_STR("save([string filename]) -> None\n\n"
		 "Save data back (optionally to a different file).")},
	{"close",	(PyCFunction)close_it,	METH_VARARGS,
	 PyDoc_STR("close() -> None\n\n"
		   "Close file (note, does not save!).")},
	{"add_dataset",	(PyCFunction)add_dataset,	 METH_VARARGS,
	 PyDoc_STR("add_dataset((int record, int tag)) -> DataSet\n\n"
		   "Add a new, empty, dataset with (record, tag) value.")},
	{NULL, NULL}, /* sentinel */
};

PyTypeObject Data_Type = {
	/* The ob_type field must be initialized in the module init function
	 * to be portable to Windows without using C++. */
	PyObject_HEAD_INIT(NULL)
	0,			/*ob_size*/
	"iptcdatamodule.Data",	/*tp_name*/
	sizeof(DataObject),	/*tp_basicsize*/
	0,			/*tp_itemsize*/
	/* methods */
	(destructor)dealloc,    /*tp_dealloc*/
	0,			/*tp_print*/
	0,			/*tp_getattr*/
	0, 			/*tp_setattr*/
	0,			/*tp_compare*/
	0,			/*tp_repr*/
	0,			/*tp_as_number*/
	0,			/*tp_as_sequence*/
	0,			/*tp_as_mapping*/
	0,			/*tp_hash*/
        0,                      /*tp_call*/
        0,                      /*tp_str*/
        0,                      /*tp_getattro*/
        0,                      /*tp_setattro*/
        0,                      /*tp_as_buffer*/
        Py_TPFLAGS_DEFAULT,     /*tp_flags*/
        0,                       /*tp_doc*/
        0,                      /*tp_traverse*/
        0,                      /*tp_clear*/
        0,                      /*tp_richcompare*/
        0,                      /*tp_weaklistoffset*/
        0,                      /*tp_iter*/
        0,                      /*tp_iternext*/
        methods,	        /*tp_methods*/
        0,		        /*tp_members*/
        getseters,              /*tp_getset*/
        0,                      /*tp_base*/
        0,                      /*tp_dict*/
        0,                      /*tp_descr_get*/
        0,                      /*tp_descr_set*/
        0,                      /*tp_dictoffset*/
        0,                      /*tp_init*/
        0,                      /*tp_alloc*/
	/* this is instantiated by the open function */
        0,                      /*tp_new*/
        0,                      /*tp_free*/
        0,                      /*tp_is_gc*/
};
/* --------------------------------------------------------------------- */

