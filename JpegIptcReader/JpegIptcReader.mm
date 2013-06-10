/*
 *   Copyright 2012 - 2013 Kulykov Oleh
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */


#import "JpegIptcReader.h"

#include "jpeglib.h"
#include "iptc-jpeg.h"
#include "iptc-data.h"
#include "iptc-dataset.h"
#include "iptc-tag.h"

@implementation JpegIptcReader

+ (void) readBinary:(IptcDataSet *)e
			withKey:(NSString *)k
			andDict:(NSMutableDictionary *)dict
{
	const unsigned int maxSize = 5 * 1024 * 1024;
	unsigned char buf[maxSize];
	const int c = iptc_dataset_get_data(e, buf, sizeof(buf));
	if (c > 0)
	{
		NSData * data = [NSData dataWithBytes:buf length:c];
		if (data)
		{
			[dict setObject:data forKey:k];
		}
	}
}

+ (void) readString:(IptcDataSet *)e 
			withKey:(NSString *)k
			andDict:(NSMutableDictionary *)dict
{
	const unsigned int maxSize = 1024 * 512 + 1;
	char buf[maxSize] = { 0 };
	iptc_dataset_get_as_str(e, buf, maxSize);
	NSString * v = [NSString stringWithUTF8String:buf];
	if (v)
	{
		[dict setObject:v forKey:k];
	}
}

+ (NSDictionary *) readAllTags:(IptcData *)d
{
    NSMutableDictionary * dict = [NSMutableDictionary dictionaryWithCapacity:d->count];
	for (int i = 0; i < d->count; i++) 
	{
		IptcDataSet * e = d->datasets[i];
		if (e)
		{
			NSString * k = [NSString stringWithUTF8String:iptc_tag_get_title(e->record, e->tag)];
			if (k)
			{
				switch (iptc_dataset_get_format(e)) 
				{
					case IPTC_FORMAT_BYTE:
					case IPTC_FORMAT_SHORT:
					case IPTC_FORMAT_LONG:
					{
						NSNumber * num = [NSNumber numberWithUnsignedInt:iptc_dataset_get_value(e)];
						if (num)
						{
							[dict setObject:num forKey:k]; 
						}
					}
						break;
						
					case IPTC_FORMAT_STRING:
						[self readString:e withKey:k andDict:dict];
						break;
						
					case IPTC_FORMAT_BINARY:
						[self readBinary:e withKey:k andDict:dict];
						break;
						
					default:
						[self readBinary:e withKey:k andDict:dict];
						break;
				}
			}
		}
	}
    return dict;
}

+ (NSDictionary *) readFromBuff:(unsigned char *)fileBuff
					andFileSize:(unsigned long)fileBuffSize
{
	struct jpeg_decompress_struct cinfo = { 0 };
	struct jpeg_error_mgr pub = { 0 };
	int ps3_pos = 0;
	unsigned int ps3_len = 0;
	jmp_buf setjmp_buffer;
	IptcData * d = 0;
	
	cinfo.err = jpeg_std_error(&pub);
	if ( setjmp(setjmp_buffer) )
	{
		jpeg_destroy_decompress(&cinfo);
		return nil;
	}
	
	jpeg_create_decompress(&cinfo);
	jpeg_mem_src(&cinfo, fileBuff, fileBuffSize);
	/* be sure to save the APP13 header, which might contain IPTC data */
	jpeg_save_markers (&cinfo, JPEG_APP0+13, 0xffff);
	jpeg_read_header(&cinfo, true);
	
	/* look for an IPTC header */
	jpeg_saved_marker_ptr marker = cinfo.marker_list;
	while (marker) 
	{
		if (marker->marker == JPEG_APP0+13) 
		{
			ps3_pos = iptc_jpeg_ps3_find_iptc(marker->data, marker->data_length, &ps3_len);
			if (ps3_pos > 0)
			{
				break;
			}
		}
		marker = marker->next;
	}
	
	if (!marker) 
	{
		/* clean up if we don't find IPTC data */
		if (d) 
		{
			iptc_data_unref(d); 
		}
		jpeg_destroy_decompress(&cinfo);
		return nil;
	}
	
	/* parse the IPTC data */
	d = iptc_data_new_from_data(marker->data + ps3_pos, ps3_len);
	if (!d) 
	{
		jpeg_destroy_decompress(&cinfo);
		return nil;
	}
	
	NSDictionary * dict = d->count > 0 ? [self readAllTags:d] : nil;
	
	if (d)
	{
		iptc_data_unref(d);
	}
	
	jpeg_destroy_decompress(&cinfo);
	
	return (dict && [dict count] > 0) ? dict : nil;
}

+ (NSDictionary *) allTagsPropertiesFromFullFilePath:(NSString *)fullFilePath
{
	NSData * data = [NSData dataWithContentsOfFile:fullFilePath];
	if (data)
	{
		if ([data length] > 0)
		{
			try 
			{
				NSDictionary * dict = [JpegIptcReader readFromBuff:(unsigned char *)[data bytes] 
													   andFileSize:(unsigned long)[data length]];
				return dict;
			}
			catch (...) 
			{
				return nil;
			}
		}
	}
	return nil;
}

@end
