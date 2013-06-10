/* pyiptcdatamod.c -- functions for the iptcdata Python module
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

PyMODINIT_FUNC initiptcdata(void);

PyDoc_STRVAR(module_doc,
	     "Routines to query, modify and add IPTC metadata to a JPEG file\n\n"
	     "Module for querying, modifying and adding IPTC metadata to a JPEG file\n"
	     "Usage is as follows:\n"
	     "\n - open your image with iptcdata.open()\n"
	     "    f = iptcdata.open(\"/path/to/image\")\n"
	     "\n - existing data is an array called 'datasets'\n"
	     "    >>> len(f.datasets)\n"
	     "    6\n"
	     "    >>> str(f.datasets[3])\n"
	     "    '2:25|Keywords -> hello, world'\n"
	     "\n - values are available of attributes of each dataset object\n"
	     "    >>> f.datasets[3].tag\n"
	     "    25\n"
	     "    >>> f.datasets[3].record\n"
	     "    2\n"
	     "    >>> f.datasets[3].title\n"
	     "    'Keywords'\n"
	     "    >>> f.datasets[3].value\n"
	     "    'hello, world'\n"
	     "\n - these attributes can be updated\n"
	     "    >>> f.datasets[3].value = 'another value'\n"
	     "\n - datasets can be deleted\n"
	     "    >>> f.datasets[3].delete()\n"
	     "\n - updated or deleted values are only written when the file is saved, optionally to a new file\n"
	     "    >>> f.save(filename='/a/new/file.jpg')\n"
	     "\n - the file should be closed when you are finished; after closing you can no longer access attributes\n"
	     "    >>> f.close()\n"
	);

/* Function of no arguments returning new Data object */
static PyObject *
open_file(PyObject *self, PyObject *args)
{
	char *filename;
	int fd;
	DataObject *data_obj;
	unsigned char file_hdr[2], jpeg_hdr[2] = {0xff,0xd8};

	if (!PyArg_ParseTuple(args, "s:new", &filename))
		return NULL;

	/* check if the file exists first.  We close this fd just
	 * because, but maybe in the future there is a case for
	 * keeping it around. */
	fd = open(filename, O_RDONLY);
	if (fd < 0)
		return PyErr_SetFromErrnoWithFilename(PyExc_IOError, filename);

	/* read the first 2 bytes, and check it looks like a jpeg */
	if (read(fd, file_hdr, 2) < 2)	{
		close(fd);
		return PyErr_SetFromErrnoWithFilename(PyExc_IOError, filename);
	}
	if (memcmp(jpeg_hdr, file_hdr, 2) != 0)	{
		close(fd);
		PyErr_SetString(PyExc_ValueError,
				"This file does not appear to be a JPEG file\n");
		return NULL;
	}
	close(fd);

	data_obj = newDataObject(args);
	if (data_obj == NULL)
		return PyErr_NoMemory();

	/* save the filename for later */
	data_obj->filename = PyString_FromString(filename);
	if (!data_obj->filename) {
		Py_DECREF(data_obj);
		return PyErr_NoMemory();
	}

	/* firstly, try and get the existing data */
	data_obj->d = iptc_data_new_from_jpeg(filename);
	if (data_obj->d) {
		/* read the existing iptc data into the dataset objects */
		int i;
		for (i=0; i < data_obj->d->count; i++) {
			IptcDataSet *e = data_obj->d->datasets[i];
			DataSetObject *ds = newDataSetObject(e);
			/* XXX bail out? */

			/* dataset objects hold a reference to their
			 * parent dataobject */
			ds->parent = data_obj;
			Py_INCREF(data_obj);

			ds->state = VALID;

			PyList_Append(data_obj->DataSet_list, (PyObject *)ds);
		}
	} else {
		/* create a new, empty data object */
		data_obj->d = iptc_data_new();
		if (!data_obj->d)
			return PyErr_NoMemory();
	}

	data_obj->state = OPEN;

	return (PyObject*)data_obj;
}

static PyObject *
get_tag_description(PyObject *self, PyObject *args, PyObject *keywds)
{
	int record, tag;
	static char *kwlist[] = {"record", "tag", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, keywds, "ii", kwlist,
					 &record, &tag))
		return NULL;

	return Py_BuildValue("s", iptc_tag_get_description(record, tag));
}

static PyObject *
find_record_by_name(PyObject *self, PyObject *args, PyObject *keywds)
{
	char *name = NULL;
	IptcRecord record;
	IptcTag tag;
	char *kwlist[] = {"name", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, keywds, "s", kwlist, &name))
		return NULL;

	if (iptc_tag_find_by_name(name, &record, &tag) < 0)
	{
		PyErr_SetString(PyExc_ValueError,
				"Record not found");
		return NULL;
	}

	return Py_BuildValue("(ii)", record, tag);
}

/* List of functions defined in the module */

static PyMethodDef methods[] = {
	{"open",	open_file,		METH_VARARGS,
	 PyDoc_STR("open(filename) -> Data\n\n"
		   "Open a JPEG and return a data object representing the IPTC data") },

	{"find_record_by_name",	(PyCFunction)find_record_by_name, METH_VARARGS|METH_KEYWORDS,
	 PyDoc_STR("find_record_by_name(name) -> (int record, int tag)\n\n"
		   "Find a record and tag value from a string argument") },

	{"get_tag_description",	(PyCFunction)get_tag_description, METH_VARARGS|METH_KEYWORDS,
	 PyDoc_STR("get_tag_description(record, tag) -> String\n\n"
		   "Get a textual description of a given record and tag") },

	{NULL,		NULL}		/* sentinel */
};

/* Initialization function for the module (*must* be called initiptcdata) */
PyMODINIT_FUNC
initiptcdata(void)
{
	PyObject *m;

	/* Finalize the type object including setting type of the new type
	 * object; doing it here is required for portability to Windows
	 * without requiring C++. */
	if (PyType_Ready(&Data_Type) < 0)
		return;

	if (PyType_Ready(&DataSet_Type) < 0)
		return;

	/* Create the module and add the functions */
	m = Py_InitModule3("iptcdata", methods, module_doc);
	if (m == NULL)
		return;

	PyModule_AddObject(m, "Data", (PyObject*)&Data_Type);
	PyModule_AddObject(m, "DataSet", (PyObject*)&DataSet_Type);

	/* IPTC constants */
#define ADD_AS_INT_CONSTANT(M, CONSTANT) PyModule_AddIntConstant(M, #CONSTANT, CONSTANT)

	/* Record types */
	ADD_AS_INT_CONSTANT(m, IPTC_RECORD_OBJECT_ENV);
	ADD_AS_INT_CONSTANT(m, IPTC_RECORD_APP_2);
	ADD_AS_INT_CONSTANT(m, IPTC_RECORD_APP_3);
	ADD_AS_INT_CONSTANT(m, IPTC_RECORD_APP_4);
	ADD_AS_INT_CONSTANT(m, IPTC_RECORD_APP_5);
	ADD_AS_INT_CONSTANT(m, IPTC_RECORD_APP_6);
	ADD_AS_INT_CONSTANT(m, IPTC_RECORD_PREOBJ_DATA);
	ADD_AS_INT_CONSTANT(m, IPTC_RECORD_OBJ_DATA);
	ADD_AS_INT_CONSTANT(m, IPTC_RECORD_POSTOBJ_DATA);

	/* Tag types */
	/* Begin record 1 tags */
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_MODEL_VERSION);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_DESTINATION);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_FILE_FORMAT);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_FILE_VERSION);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_SERVICE_ID);
        ADD_AS_INT_CONSTANT(m, IPTC_TAG_ENVELOPE_NUM);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_PRODUCT_ID);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_ENVELOPE_PRIORITY);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_DATE_SENT);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_TIME_SENT);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_CHARACTER_SET);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_UNO);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_ARM_ID);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_ARM_VERSION);
	/* End record 1 tags */
	/* Begin record 2 tags */
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_RECORD_VERSION);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_OBJECT_TYPE);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_OBJECT_ATTRIBUTE);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_OBJECT_NAME);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_EDIT_STATUS);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_EDITORIAL_UPDATE);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_URGENCY);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_SUBJECT_REFERENCE);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_CATEGORY);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_SUPPL_CATEGORY);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_FIXTURE_ID);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_KEYWORDS);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_CONTENT_LOC_CODE);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_CONTENT_LOC_NAME);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_RELEASE_DATE);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_RELEASE_TIME);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_EXPIRATION_DATE);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_EXPIRATION_TIME);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_SPECIAL_INSTRUCTIONS);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_ACTION_ADVISED);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_REFERENCE_SERVICE);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_REFERENCE_DATE);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_REFERENCE_NUMBER);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_DATE_CREATED);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_TIME_CREATED);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_DIGITAL_CREATION_DATE);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_DIGITAL_CREATION_TIME);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_ORIGINATING_PROGRAM);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_PROGRAM_VERSION);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_OBJECT_CYCLE);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_BYLINE);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_BYLINE_TITLE);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_CITY);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_SUBLOCATION);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_STATE);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_COUNTRY_CODE);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_COUNTRY_NAME);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_ORIG_TRANS_REF);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_HEADLINE);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_CREDIT);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_SOURCE);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_COPYRIGHT_NOTICE);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_PICASA_UNKNOWN);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_CONTACT);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_CAPTION);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_WRITER_EDITOR);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_RASTERIZED_CAPTION);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_IMAGE_TYPE);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_IMAGE_ORIENTATION);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_LANGUAGE_ID);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_AUDIO_TYPE);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_AUDIO_SAMPLING_RATE);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_AUDIO_SAMPLING_RES);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_AUDIO_DURATION);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_AUDIO_OUTCUE);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_PREVIEW_FORMAT);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_PREVIEW_FORMAT_VER);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_PREVIEW_DATA);
	/* End record 2 tags */
	/* Begin record 7 tags */
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_SIZE_MODE);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_MAX_SUBFILE_SIZE);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_SIZE_ANNOUNCED);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_MAX_OBJECT_SIZE);
	/* End record 7 tags */
	/* Record 8 tags */
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_SUBFILE);
	ADD_AS_INT_CONSTANT(m, IPTC_TAG_CONFIRMED_DATA_SIZE);
	/* Record 9 tags */
}
