/* pyiptcdata.h -- generic includes for iptcdata Python module
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

#include "Python.h"
#include "structmember.h"

#include "datetime.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <libiptcdata/iptc-data.h>
#include <libiptcdata/iptc-jpeg.h>

/* buffer length for IPTC_DATA */
#define PS3_BUFLEN (256*256)

/* typedef for a data object, which essentially holds a list of
   dataset object representing the actual IPTC data */
typedef struct {
	PyObject_HEAD
	PyObject	*filename;
	IptcData 	*d;		/* Image data		 */
	PyObject	*DataSet_list;
	enum {OPEN, CLOSED} state;
} DataObject;
extern PyTypeObject Data_Type;


/* typedef for a dataset object */
typedef struct {
	PyObject_HEAD
	IptcDataSet *ds;
	DataObject *parent;
	enum {VALID, INVALID} state;
} DataSetObject;
extern PyTypeObject DataSet_Type;

DataObject *newDataObject(PyObject *arg);
DataSetObject *newDataSetObject(IptcDataSet *ds);
