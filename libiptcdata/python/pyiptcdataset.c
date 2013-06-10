/* pyiptcdataset.c -- functions describiing a  DataSet object
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

/* this stops us setting values on closed object */
#define check_parent_open(dataset_obj, ret)					\
if (dataset_obj->parent->state == CLOSED) {					\
	PyErr_SetString(PyExc_ValueError, "operation on closed dataset");	\
	return ret;								\
}

/* check we haven't deleted this dataset */
#define check_valid(dataset_obj, ret)						\
if (dataset_obj->state == INVALID) {						\
	PyErr_SetString(PyExc_ValueError, "operation on invalid dataset");	\
	return ret;								\
}

DataSetObject *
newDataSetObject(IptcDataSet *ds)
{
	DataSetObject *self;
	self = PyObject_New(DataSetObject, &DataSet_Type);
	if (self == NULL)
		return NULL;
	self->ds = ds;
	return self;
}

static void
dealloc(DataSetObject *self)
{
	/* We don't call iptc_dataset_unref(self->ds) here because
	 * iptc_data_unref will walk through the children and free
	 * them as appropriate. */

	/* remove our reference from our parent */
	Py_DECREF(self->parent);

	/* free ourself, as per Python tutorial */
	self->ob_type->tp_free(self);
}

/* TITLE */
static PyObject *
get_title(DataSetObject *self, void *closure)
{
	check_valid(self, NULL);
	return Py_BuildValue("s", iptc_tag_get_title(self->ds->record, self->ds->tag));
}

/* DESCRIPTION */
static PyObject *
get_description(DataSetObject *self, void *closure)
{
	check_valid(self, NULL);
	return Py_BuildValue("s", iptc_tag_get_description(self->ds->record, self->ds->tag));
}

/* VALUE */
static PyObject *
get_value(DataSetObject *self, void *closure)
{
	PyObject *ret = NULL;
	char buf[256];

	check_valid(self, NULL);

	switch (iptc_dataset_get_format (self->ds)) {
	case IPTC_FORMAT_BYTE:
	case IPTC_FORMAT_SHORT:
	case IPTC_FORMAT_LONG:
		ret = Py_BuildValue("i", iptc_dataset_get_value(self->ds));
		break;
	case IPTC_FORMAT_BINARY:
		iptc_dataset_get_as_str(self->ds, buf, sizeof(buf));
		ret = Py_BuildValue("s", buf);
		break;
	default:
		iptc_dataset_get_as_str(self->ds, buf, sizeof(buf));
		ret = Py_BuildValue("s", buf);
		break;
	}
	return ret;
}

static int
set_value(DataSetObject *self, PyObject *value, void *closure)
{
	int ok;
	long byte_val=0;

	char *binary_val = NULL;
	int binary_len = 0;

	check_valid(self, -1);
	check_parent_open(self, -1);

	switch (iptc_dataset_get_format (self->ds)) {
	case IPTC_FORMAT_BYTE:
	case IPTC_FORMAT_SHORT:
	case IPTC_FORMAT_LONG:
		if (!PyInt_Check(value)) {
			PyErr_SetString(PyExc_TypeError,
					"The value of this attribute must be an integer");
			return -1;
		}
		ok = PyArg_ParseTuple(value, "i", &byte_val);
		if (!ok) {
			PyErr_SetString(PyExc_TypeError,
					"Invalid value for integer!");
			return -1;
		}
		ok = iptc_dataset_set_value(self->ds, byte_val, IPTC_VALIDATE);
		if (ok == -1) {
			PyErr_SetString(PyExc_TypeError,
					"iptc_dataset_set_value failed");
			return -1;
		}
		return 0;
	case IPTC_FORMAT_BINARY:
	default:
		if (!PyString_Check(value)) {
			PyErr_SetString(PyExc_TypeError,
					"The value of this attribute must be a string");
			return -1;
		}
		binary_val = PyString_AsString(value);
		binary_len = strlen(binary_val);
		ok = iptc_dataset_set_data(self->ds, (unsigned char *) binary_val, binary_len, IPTC_VALIDATE);
		if (ok == -1) {
			PyErr_SetString(PyExc_TypeError,
					"iptc_dataset_set_data failed");
			return -1;
		}
	}

	return 0;
}

/* RECORD */
static PyObject *
get_record(DataSetObject *self, void *closure)
{
	check_valid(self, NULL);
	return Py_BuildValue("i", self->ds->record);
}

/* TAG */
static PyObject *
get_tag(DataSetObject *self, void *closure)
{
	check_valid(self, NULL);
	return Py_BuildValue("i", self->ds->tag);
}

/* TIME
 * Return an exception if the time isn't set */
static PyObject *
get_time(DataSetObject *self, void *closure)
{
	int year=0, month=0, day=0, hour=0, minute=0, second=0, tz=0;
	int ret;

	check_valid(self, NULL);

	ret = iptc_dataset_get_date(self->ds, &year, &month, &day);
	if (ret == -1) {
		PyErr_SetString(PyExc_ValueError,
				"Can not get year/month/day information");
		return NULL;
	}
	ret = iptc_dataset_get_time(self->ds, &hour, &minute, &second, &tz);
	if (ret == -1) {
		PyErr_SetString(PyExc_ValueError,
				"Can not get hour/min/sec information");
		return NULL;
	}
	PyDateTime_IMPORT;
	return PyDateTime_FromDateAndTime(year, month, day, hour, minute, second, 0);
}

/* XXX something to think about is how to get timezone information in
 * here; the Python datetime object doesn't include it. */
static PyObject *
set_time(DataSetObject *self, PyObject *value, void *closure)
{
	int ret;

	check_valid(self, NULL);
	check_parent_open(self, NULL);

	PyDateTime_IMPORT;

	if (!PyDate_Check(value)) {
		PyErr_SetString(PyExc_TypeError,
				"You must pass at datetime object");
		return NULL;
	}
	ret = iptc_dataset_set_date(self->ds,
				    PyDateTime_GET_YEAR(value),
				    PyDateTime_GET_MONTH(value),
				    PyDateTime_GET_DAY(value),
				    IPTC_VALIDATE);
	if (ret == 0) {
		PyErr_SetString(PyExc_TypeError,
				"Year/month/day information does not validate");
		return NULL;
	}
	if (ret == -1)	{
		PyErr_SetString(PyExc_ValueError,
				"Can not set year/month/day information");
		return NULL;
	}
	ret = iptc_dataset_set_time(self->ds,
				    PyDateTime_DATE_GET_HOUR(value),
				    PyDateTime_DATE_GET_MINUTE(value),
				    PyDateTime_DATE_GET_SECOND(value),
				    0,
				    IPTC_VALIDATE);
	if (ret == 0) {
		PyErr_SetString(PyExc_TypeError,
				"Hour/minute/second information does not validate");
		return NULL;
	}
	if (ret == -1) {
		PyErr_SetString(PyExc_ValueError,
				"Can not set hour/minute/second information");
		return NULL;
	}

	Py_RETURN_TRUE;
}

static PyObject *
to_str(DataSetObject *self)
{
	IptcDataSet *e = ((DataSetObject*)self)->ds;
	char buf[256];

	check_valid(self, NULL);

	switch (iptc_dataset_get_format (e)) {
		case IPTC_FORMAT_BYTE:
		case IPTC_FORMAT_SHORT:
		case IPTC_FORMAT_LONG:
			sprintf(buf, "%d", iptc_dataset_get_value (e));
			break;
		case IPTC_FORMAT_BINARY:
			iptc_dataset_get_as_str (e, buf, sizeof(buf));
			break;
		default:
			iptc_dataset_get_data (e, (unsigned char *) buf, sizeof(buf));
			break;
	}

	return PyString_FromFormat("%2d:%03d|%-20.20s -> %s",
			e->record, e->tag,
			iptc_tag_get_title (e->record, e->tag), buf);
}

static PyObject *
delete(DataSetObject *self, PyObject *args)
{
	int i;
	IptcDataSet *ds;

	/* first, remove from libiptcdata */
	for (i=0; i < self->parent->d->count; i++) {
		ds = self->parent->d->datasets[i];
		if ( ds == self->ds ) {
			if (iptc_data_remove_dataset(self->parent->d, ds) < 0) {
				PyErr_SetString(PyExc_ValueError,
						"Can not remove dataset");
				return NULL;
			}
			self->parent->d->datasets[i] = NULL;
		}
	}

	/* now take this object out of the list */
	for (i=0; i < PyList_Size(self->parent->DataSet_list); i++)
	{
		DataSetObject *tmp = (DataSetObject*)PyList_GetItem(self->parent->DataSet_list, i);
		if (tmp == self) {
			tmp->state = INVALID;
			PyList_SetSlice(self->parent->DataSet_list, i, i+1, NULL);
			break;
		}
	}

	/* Remove the reference this object had to the parent */
	Py_DECREF(self->parent);

	Py_RETURN_NONE;
}

static PyMethodDef methods[] = {
	{"delete",	(PyCFunction)delete,	METH_VARARGS,
	 PyDoc_STR("delete() -> None\n\n"
		   "Delete this dataset from the IPTC data.")},
	 {NULL, NULL}, /* sentinel */
};

static PyGetSetDef getseters[] = {
	{"title", (getter)get_title, NULL,
	 "Dataset title", NULL},
	{"description", (getter)get_description, NULL,
	 "Dataset description", NULL},
	{"value", (getter)get_value, (setter)set_value,
	 "Dataset value", NULL},
	{"record", (getter)get_record, NULL,
	 "Dataset record", NULL},
	{"tag", (getter)get_tag, NULL,
	 "Dataset tag", NULL},
	{"time", (getter)get_time, (setter)set_time,
	 "Dataset time", NULL},
	{NULL}
};

PyTypeObject DataSet_Type = {
	/* The ob_type field must be initialized in the module init function
	 * to be portable to Windows without using C++. */
	PyObject_HEAD_INIT(NULL)
	0,			/*ob_size*/
	"iptcdatamodule.DataSet", /*tp_name*/
	sizeof(DataSetObject),	/*tp_basicsize*/
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
        (reprfunc) to_str,      /*tp_str*/
        0,                      /*tp_getattro*/
        0,                      /*tp_setattro*/
        0,                      /*tp_as_buffer*/
        Py_TPFLAGS_DEFAULT,     /*tp_flags*/
        0,                      /*tp_doc*/
        0,                      /*tp_traverse*/
        0,                      /*tp_clear*/
        0,                      /*tp_richcompare*/
        0,                      /*tp_weaklistoffset*/
        0,                      /*tp_iter*/
        0,                      /*tp_iternext*/
        methods,                /*tp_methods*/
        0,                      /*tp_members*/
        getseters,              /*tp_getset*/
        0,                      /*tp_base*/
        0,                      /*tp_dict*/
        0,                      /*tp_descr_get*/
        0,                      /*tp_descr_set*/
        0,                      /*tp_dictoffset*/
        0,                      /*tp_init*/
        0,                      /*tp_alloc*/
        PyType_GenericNew,      /*tp_new*/
        0,                      /*tp_free*/
        0,                      /*tp_is_gc*/
};
