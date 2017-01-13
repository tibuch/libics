/*
 * libics: Image Cytometry Standard file reading and writing.
 *
 * Copyright 2015-2017:
 *   Scientific Volume Imaging Holding B.V.
 *   Laapersveld 63, 1213 VB Hilversum, The Netherlands
 *   https://www.svi.nl
 *
 * Copyright (C) 2000-2013 Cris Luengo and others
 *
 * Large chunks of this library written by
 *    Bert Gijsbers
 *    Dr. Hans T.M. van der Voort
 * And also Damir Sudar, Geert van Kempen, Jan Jitze Krol,
 * Chiel Baarslag and Fons Laan.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * FILE : libics_top.c
 *
 * The following library functions are contained in this file:
 *
 *   IcsOpen()
 *   IcsClose()
 *   IcsGetLayout()
 *   IcsSetLayout()
 *   IcsGetDataSize()
 *   IcsGetImelSize()
 *   IcsGetImageSize()
 *   IcsGetData()
 *   IcsGetDataBlock()
 *   IcsSkipDataBlock()
 *   IcsGetROIData()
 *   IcsGetDataWithStrides()
 *   IcsSetData()
 *   IcsSetDataWithStrides()
 *   IcsSetSource()
 *   IcsSetCompression()
 *   IcsGetPosition()
 *   IcsSetPosition()
 *   IcsGetOrder()
 *   IcsSetOrder()
 *   IcsGetCoordinateSystem()
 *   IcsSetCoordinateSystem()
 *   IcsGetSignificantBits()
 *   IcsSetSignificantBits()
 *   IcsGetImelUnits()
 *   IcsSetImelUnits()
 *   IcsGetScilType()
 *   IcsSetScilType()
 *   IcsGuessScilType()
 *   IcsGetErrorText()
 */


#include <stdlib.h>
#include <string.h>
#include "libics_intern.h"


/* Default Order and Label strings: */
char const* ICSKEY_ORDER[] = {"x", "y", "z", "t", "probe"};
const char * ICSKEY_LABEL[] = {"x-position", "y-position", "z-position",
                               "time", "probe"};
#define ICSKEY_ORDER_LENGTH 5 /* Number of elements in ICSKEY_ORDER and ICSKEY_LABEL arrays. */


/* Create an ICS structure, and read the stuff from file if reading. */
Ics_Error IcsOpen(ICS        **ics,
                  const char  *filename,
                  const char  *mode)
{
    ICSINIT;
    int    version = 0, forceName = 0, forceLocale = 1, reading = 0;
    int    writing = 0;
    size_t i;


        /* the mode string is one of: "r", "w", "rw", with "f" and/or "l"
           appended for reading and "1" or "2" appended for writing */
    for (i = 0; i<strlen(mode); i++) {
        switch (mode[i]) {
            case 'r':
                ICSTR(reading, IcsErr_IllParameter);
                reading = 1;
                break;
            case 'w':
                ICSTR(writing, IcsErr_IllParameter);
                writing = 1;
                break;
            case 'f':
                ICSTR(forceName, IcsErr_IllParameter);
                forceName = 1;
                break;
            case 'l':
                ICSTR(forceLocale, IcsErr_IllParameter);
                forceLocale = 0;
                break;
            case '1':
                ICSTR(version!=0, IcsErr_IllParameter);
                version = 1;
                break;
            case '2':
                ICSTR(version!=0, IcsErr_IllParameter);
                version = 2;
                break;
            default:
                return IcsErr_IllParameter;
        }
    }
    *ics =(ICS*)malloc(sizeof(ICS));
    ICSTR(*ics == NULL, IcsErr_Alloc);
    if (reading) {
            /* We're reading or updating */
        error = IcsReadIcs(*ics, filename, forceName, forceLocale);
        if (error) {
            free(*ics);
            *ics = NULL;
        } else {
            if (writing) {
                    /* We're updating */
                (*ics)->FileMode = IcsFileMode_update;
            } else {
                    /* We're just reading */
                (*ics)->FileMode = IcsFileMode_read;
            }
        }
    } else if (writing) {
            /* We're writing */
        IcsInit(*ics);
        (*ics)->FileMode = IcsFileMode_write;
        if (version) {
            (*ics)->Version = version;
        }
        IcsStrCpy((*ics)->Filename, filename, ICS_MAXPATHLEN);
    } else {
            /* Missing an "r" or "w" mode character */
        return IcsErr_IllParameter;
    }

    return error;
}


/* Free the ICS structure, and write the stuff to file if writing. */
Ics_Error IcsClose(ICS *ics)
{
    ICSINIT;
    char filename[ICS_MAXPATHLEN+4];


    ICSTR(ics == NULL, IcsErr_NotValidAction);
    if (ics->FileMode == IcsFileMode_read) {
            /* We're reading */
        if (ics->BlockRead != NULL) {
            error = IcsCloseIds(ics);
        }
    } else if (ics->FileMode == IcsFileMode_write) {
            /* We're writing */
        error = IcsWriteIcs(ics, NULL);
        ICSCX(IcsWriteIds(ics));
    } else {
            /* We're updating */
        int needcopy = 0;
        if (ics->BlockRead != NULL) {
            error = IcsCloseIds(ics);
        }
        if (ics->Version == 2 && !strcmp(ics->SrcFile, ics->Filename)) {
                /* The ICS file contains the data */
            needcopy = 1;
            ics->SrcFile[0] = '\0'; /* needed to get the END keyword in the header */
                /* Rename the original file */
            strcpy(filename, ics->Filename);
            strcat(filename, ".tmp");
            if (rename(ics->Filename, filename)) {
                error = IcsErr_FTempMoveIcs;
            }
        }
        ICSCX(IcsWriteIcs(ics, NULL));
        if (!error && needcopy) {
                /* Copy the data over from the original file */
            error = IcsCopyIds(filename, ics->SrcOffset, ics->Filename);
                /* Delete original file */
            if (!error) {
                remove(filename);
            }
        }
        if (error) {
                /* Let's try copying the old file back */
            remove(ics->Filename);
            rename(filename, ics->Filename);
        }
    }
    IcsFreeHistory(ics);
    free(ics);

    return error;
}


/* Get the layout parameters from the ICS structure. */
Ics_Error IcsGetLayout(const ICS    *ics,
                       Ics_DataType *dt,
                       int          *nDims,
                       size_t       *dims)
{
    ICSINIT;
    int i;


    ICS_FM_RD(ics);
    *nDims = ics->Dimensions;
    *dt = ics->Imel.DataType;
        /* Get the image sizes. Ignore the orders */
    for (i = 0; i < *nDims; i++) {
        dims[i] = ics->Dim[i].Size;
    }

    return error;
}


/* Put the layout parameters in the ICS structure. */
Ics_Error IcsSetLayout(ICS          *ics,
                       Ics_DataType  dt,
                       int           nDims,
                       const size_t *dims)
{
    ICSINIT;
    int i;


    ICS_FM_WD(ics);
    ICSTR(nDims > ICS_MAXDIM, IcsErr_TooManyDims);
        /* Set the pixel parameters */
    ics->Imel.DataType = dt;
        /* Set the image sizes */
    for (i=0; i<nDims; i++) {
        ics->Dim[i].Size = dims[i];
        if (i < ICSKEY_ORDER_LENGTH) {
            strcpy(ics->Dim[i].Order, ICSKEY_ORDER[i]);
            strcpy(ics->Dim[i].Label, ICSKEY_LABEL[i]);
        } else {
                /* Could overflow: */
            snprintf(ics->Dim[i].Order, ICS_STRLEN_TOKEN, "dim_%d", i);
            snprintf(ics->Dim[i].Label, ICS_STRLEN_TOKEN, "dim_%d", i);
        }
    }
    ics->Dimensions = nDims;

    return error;
}


/* Get the image size in bytes. */
size_t IcsGetDataSize(const ICS *ics)
{
    ICSTR(ics == NULL, 0);
    ICSTR(ics->Dimensions == 0, 0);
    return IcsGetImageSize(ics) * IcsGetBytesPerSample(ics);
}


/* Get the pixel size in bytes. */
size_t IcsGetImelSize(const ICS *ics)
{
    if (ics != NULL) {
        return IcsGetBytesPerSample(ics);
    } else {
        return 0;
    }
}


/* Get the image size in pixels. */
size_t IcsGetImageSize(const ICS *ics)
{
    int    i;
    size_t size = 1;


    ICSTR(ics == NULL, 0);
    ICSTR(ics->Dimensions == 0, 0);
    for (i = 0; i < ics->Dimensions; i++) {
        size *= ics->Dim[i].Size;
    }

    return size;
}


/* Get the image data. It is read from the file right here. */
Ics_Error IcsGetData(ICS    *ics,
                     void   *dest,
                     size_t  n)
{
    ICSINIT;


    ICS_FM_RD(ics);
    if ((n != 0) &&(dest != NULL)) {
        error = IcsReadIds(ics, dest, n);
    }

    return error;
}


/* Read a portion of the image data from an ICS file. */
Ics_Error IcsGetDataBlock(ICS    *ics,
                          void   *dest,
                          size_t  n)
{
    ICSINIT;


    ICS_FM_RD(ics);
    if ((n != 0) &&(dest != NULL)) {
        if (ics->BlockRead == NULL) {
            error = IcsOpenIds(ics);
        }
        ICSCX(IcsReadIdsBlock(ics, dest, n));
    }

    return error;
}


/* Skip a portion of the image from an ICS file. */
Ics_Error IcsSkipDataBlock(ICS    *ics,
                           size_t  n)
{
    ICSINIT;


    ICS_FM_RD(ics);
    if (n != 0) {
        if (ics->BlockRead == NULL) {
            error = IcsOpenIds(ics);
        }
        ICSCX(IcsSkipIdsBlock(ics, n));
    }

    return error;
}


/* Read a square region of the image from an ICS file. */
Ics_Error IcsGetROIData(ICS          *ics,
                        const size_t *offsetPtr,
                        const size_t *sizePtr,
                        const size_t *samplingPtr,
                        void         *destPtr,
                        size_t        n)
{
    ICSDECL;
    int           i, sizeConflict = 0, p;
    size_t        j;
    size_t        imelSize, roiSize, curLoc, newLoc, bufSize;
    size_t        curPos[ICS_MAXDIM];
    size_t        stride[ICS_MAXDIM];
    size_t        bOffset[ICS_MAXDIM];
    size_t        bSize[ICS_MAXDIM];
    size_t        bSampling[ICS_MAXDIM];
    const size_t *offset, *size, *sampling;
    char         *buf;
    char         *dest            = (char*)destPtr;


    ICS_FM_RD(ics);
    ICSTR((n == 0) ||(dest == NULL), IcsErr_Ok);
    p = ics->Dimensions;
    if (offsetPtr != NULL) {
        offset = offsetPtr;
    } else {
        for (i = 0; i < p; i++) {
            bOffset[i] = 0;
        }
        offset = bOffset;
    }
    if (sizePtr != NULL) {
        size = sizePtr;
    } else {
        for (i = 0; i < p; i++) {
            bSize[i] = ics->Dim[i].Size - offset[i];
        }
        size = bSize;
    }
    if (samplingPtr != NULL) {
        sampling = samplingPtr;
    } else {
        for (i = 0; i < p; i++) {
            bSampling[i] = 1;
        }
        sampling = bSampling;
    }
    for (i = 0; i < p; i++) {
        if (sampling[i] < 1 || offset[i] + size[i] > ics->Dim[i].Size)
            return IcsErr_IllegalROI;
    }
    imelSize = IcsGetBytesPerSample(ics);
    roiSize = imelSize;
    for (i = 0; i < p; i++) {
        roiSize *= (size[i] + sampling[i] - 1) / sampling[i];
    }
    if (n != roiSize) {
        sizeConflict = 1;
        ICSTR(n < roiSize, IcsErr_BufferTooSmall);
    }
        /* The stride array tells us how many imels to skip to go the next pixel
           in each dimension */
    stride[0] = 1;
    for (i = 1; i < p; i++) {
        stride[i] = stride[i - 1] * ics->Dim[i - 1].Size;
    }
    ICSXR(IcsOpenIds(ics));
    bufSize = imelSize*size[0];
    if (sampling[0] > 1) {
            /* We read a line in a buffer, and then copy the needed imels to
               dest */
        buf =(char*)malloc(bufSize);
        ICSTR(buf == NULL, IcsErr_Alloc);
        curLoc = 0;
        for (i = 0; i < p; i++) {
            curPos[i] = offset[i];
        }
        while (1) {
            newLoc = 0;
            for (i = 0; i < p; i++) {
                newLoc += curPos[i] * stride[i];
            }
            newLoc *= imelSize;
            if (curLoc < newLoc) {
                error = IcsSkipIdsBlock(ics, newLoc - curLoc);
                curLoc = newLoc;
            }
            ICSCX(IcsReadIdsBlock(ics, buf, bufSize));
            if (error != IcsErr_Ok) {
                break; /* stop reading on error */
            }
            curLoc += bufSize;
            for (j=0; j < size[0]; j += sampling[0]) {
                memcpy(dest, buf + i * imelSize, imelSize);
                dest += imelSize;
            }
            for (i = 1; i < p; i++) {
                curPos[i] += sampling[i];
                if (curPos[i] < offset[i] + size[i]) {
                    break;
                }
                curPos[i] = offset[i];
            }
            if (i==p) {
                break; /* we're done reading */
            }
        }
        free(buf);
    } else {
            /* No subsampling in dim[0] required: read directly into dest */
        curLoc = 0;
        for (i = 0; i < p; i++) {
            curPos[i] = offset[i];
        }
        while (1) {
            newLoc = 0;
            for (i = 0; i < p; i++) {
                newLoc += curPos[i] * stride[i];
            }
            newLoc *= imelSize;
            if (curLoc < newLoc) {
                error = IcsSkipIdsBlock(ics, newLoc - curLoc);
                curLoc = newLoc;
            }
            ICSCX(IcsReadIdsBlock(ics, dest, bufSize));
            if (error != IcsErr_Ok) {
                break; /* stop reading on error */
            }
            curLoc += bufSize;
            dest += bufSize;
            for (i = 1; i < p; i++) {
                curPos[i] += sampling[i];
                if (curPos[i] < offset[i] + size[i]) {
                    break;
                }
                curPos[i] = offset[i];
            }
            if (i==p) {
                break; /* we're done reading */
            }
        }
    }
    ICSXA(IcsCloseIds(ics));

    if ((error == IcsErr_Ok) && sizeConflict) {
        error = IcsErr_OutputNotFilled;
    }
    return error;
}


/* Read the image data into a region of your buffer. */
Ics_Error IcsGetDataWithStrides(ICS          *ics,
                                void         *destPtr,
                                size_t        n,
                                const size_t *stridePtr,
                                int           nDims)
{
   ICSDECL;
   int           i, p;
   size_t        j;
   size_t        imelSize, lastpixel, bufSize;
   size_t        curPos[ICS_MAXDIM];
   size_t        b_stride[ICS_MAXDIM];
   size_t const *stride;
   char         *buf;
   char         *dest = (char*)destPtr;
   char         *out;


   ICS_FM_RD(ics);
   ICSTR((n == 0) ||(dest == NULL), IcsErr_Ok);
   p = ics->Dimensions;
   ICSTR(nDims != p, IcsErr_IllParameter);
   if (stridePtr != NULL) {
      stride = stridePtr;
   } else {
      b_stride[0] = 1;
      for (i = 1; i < p; i++) {
         b_stride[i] = b_stride[i - 1] * ics->Dim[i - 1].Size;
      }
      stride = b_stride;
   }
   imelSize = IcsGetBytesPerSample(ics);
   lastpixel = 0;
   for (i = 0; i < p; i++) {
      lastpixel +=(ics->Dim[i].Size - 1) * stride[i];
   }
   ICSTR(lastpixel * imelSize > n, IcsErr_IllParameter);

   ICSXR(IcsOpenIds(ics));
   bufSize = imelSize*ics->Dim[0].Size;
   if (stride[0] > 1) {
      /* We read a line in a buffer, and then copy the imels to dest */
      buf =(char*)malloc(bufSize);
      ICSTR(buf == NULL, IcsErr_Alloc);
      for (i = 0; i < p; i++) {
         curPos[i] = 0;
      }
      while (1) {
         out = dest;
         for (i = 1; i < p; i++) {
            out += curPos[i] * stride[i] * imelSize;
         }
         ICSCX(IcsReadIdsBlock(ics, buf, bufSize));
         if (error != IcsErr_Ok) {
            break; /* stop reading on error */
         }
         for (j = 0; j < ics->Dim[0].Size; j++) {
            memcpy(out, buf + j * imelSize, imelSize);
            out += stride[0]*imelSize;
         }
         for (i = 1; i < p; i++) {
            curPos[i]++;
            if (curPos[i] < ics->Dim[i].Size) {
               break;
            }
            curPos[i] = 0;
         }
         if (i==p) {
            break; /* we're done reading */
         }
      }
      free(buf);
   } else {
      /* No subsampling in dim[0] required: read directly into dest */
      for (i = 0; i < p; i++) {
         curPos[i] = 0;
      }
      while (1) {
         out = dest;
         for (i = 1; i < p; i++) {
            out += curPos[i] * stride[i] * imelSize;
         }
         ICSCX(IcsReadIdsBlock(ics, out, bufSize));
         if (error != IcsErr_Ok) {
            break; /* stop reading on error */
         }
         for (i = 1; i < p; i++) {
            curPos[i]++;
            if (curPos[i] < ics->Dim[i].Size) {
               break;
            }
            curPos[i] = 0;
         }
         if (i==p) {
            break; /* we're done reading */
         }
      }
   }
   ICSXA(IcsCloseIds(ics));

   return error;
}


/* Set the image data. The pointer must be valid until IcsClose() is called. */
Ics_Error IcsSetData(ICS        *ics,
                     const void *src,
                     size_t      n)
{
   ICSINIT;


   ICS_FM_WD(ics);
   ICSTR(ics->SrcFile[0] != '\0', IcsErr_DuplicateData);
   ICSTR(ics->Data != NULL, IcsErr_DuplicateData);
   ICSTR(ics->Dimensions == 0, IcsErr_NoLayout);
   if (n != IcsGetDataSize(ics)) {
      error = IcsErr_FSizeConflict;
   }
   ics->Data = src;
   ics->DataLength = n;
   ics->DataStrides = NULL;

   return error;
}


/* Set the image data. The pointers must be valid until IcsClose() is
   called. The strides indicate how to go to the next neighbor along each
   dimension. Use this is your image data is not in one contiguous block or you
   want to swap some dimensions in the file. nDims is the length of the strides
   array and should match the dimensionality previously given. */
Ics_Error IcsSetDataWithStrides(ICS          *ics,
                                const void   *src,
                                size_t        n,
                                const size_t *strides,
                                int           nDims)
{
   ICSINIT;
   size_t lastpixel;
   int    i;


   ICS_FM_WD(ics);
   ICSTR(ics->SrcFile[0] != '\0', IcsErr_DuplicateData);
   ICSTR(ics->Data != NULL, IcsErr_DuplicateData);
   ICSTR(ics->Dimensions == 0, IcsErr_NoLayout);
   ICSTR(nDims != ics->Dimensions, IcsErr_IllParameter);
   lastpixel = 0;
   for (i = 0; i < nDims; i++) {
      lastpixel +=(ics->Dim[i].Size-1) * strides[i];
   }
   ICSTR(lastpixel*IcsGetDataTypeSize(ics->Imel.DataType) > n,
         IcsErr_IllParameter);
   if (n != IcsGetDataSize(ics)) {
      error = IcsErr_FSizeConflict;
   }
   ics->Data = src;
   ics->DataLength = n;
   ics->DataStrides = strides;

   return error;
}


/* Set the image data source file. */
Ics_Error IcsSetSource(ICS        *ics,
                       const char *fname,
                       size_t      offset)
{
   ICSINIT;


   ICS_FM_WD(ics);
   ICSTR(ics->Version == 1, IcsErr_NotValidAction);
   ICSTR(ics->SrcFile[0] != '\0', IcsErr_DuplicateData);
   ICSTR(ics->Data != NULL, IcsErr_DuplicateData);
   IcsStrCpy(ics->SrcFile, fname, ICS_MAXPATHLEN);
   ics->SrcOffset = offset;

   return error;
}


/* Set the compression method and compression parameter. */
Ics_Error IcsSetCompression(ICS             *ics,
                            Ics_Compression  compression,
                            int              level)
{
    ICSINIT;


    ICS_FM_WD(ics);
    if (compression == IcsCompr_compress)
        compression = IcsCompr_gzip; /* don't try writing 'compress' compressed
                                        data. */
    ics->Compression = compression;
    ics->CompLevel = level;

    return error;
}


/* Get the position of the image in the real world: the origin of the first
   pixel, the distances between pixels and the units in which to measure. If you
   are not interested in one of the parameters, set the pointer to
   NULL. Dimensions start at 0. */
Ics_Error IcsGetPosition(const ICS *ics,
                         int        dimension,
                         double    *origin,
                         double    *scale,
                         char      *units)
{
    ICSINIT;


    ICS_FM_RMD(ics);
    ICSTR(dimension >= ics->Dimensions, IcsErr_NotValidAction);
    if (origin) {
        *origin = ics->Dim[dimension].Origin;
    }
    if (scale) {
        *scale = ics->Dim[dimension].Scale;
    }
    if (units) {
        if (ics->Dim[dimension].Unit[0] != '\0') {
            strcpy(units, ics->Dim[dimension].Unit);
        } else {
            strcpy(units, ICS_UNITS_UNDEFINED);
        }
    }

    return error;
}


/* Set the position of the image in the real world: the origin of the first
   pixel, the distances between pixels and the units in which to measure. If
   units is NULL or empty, it is set to the default value of
   "undefined". Dimensions start at 0. */
Ics_Error IcsSetPosition(ICS        *ics,
                         int         dimension,
                         double      origin,
                         double      scale,
                         const char *units)
{
    ICSINIT;


    ICS_FM_WMD(ics);
    ICSTR(dimension >= ics->Dimensions, IcsErr_NotValidAction);
    ics->Dim[dimension].Origin = origin;
    ics->Dim[dimension].Scale = scale;
    if (units &&(units[0] != '\0')) {
        IcsStrCpy(ics->Dim[dimension].Unit, units, ICS_STRLEN_TOKEN);
    } else {
        strcpy(ics->Dim[dimension].Unit, ICS_UNITS_UNDEFINED);
    }

    return error;
}


/* Get the ordering of the dimensions in the image. The ordering is defined by
   names and labels for each dimension. The defaults are x, y, z, t(time) and
   probe. Dimensions start at 0. */
Ics_Error IcsGetOrder(const ICS *ics,
                      int        dimension,
                      char      *order,
                      char      *label)
{
    ICSINIT;


    ICS_FM_RMD(ics);
    ICSTR(dimension >= ics->Dimensions, IcsErr_NotValidAction);
    if (order) {
        strcpy(order, ics->Dim[dimension].Order);
    }
    if (label) {
        strcpy(label, ics->Dim[dimension].Label);
    }

    return error;
}


/* Set the ordering of the dimensions in the image. The ordering is defined by
   providing names and labels for each dimension. The defaults are x, y, z, t
  (time) and probe. Dimensions start at 0. */
Ics_Error IcsSetOrder(ICS        *ics,
                      int         dimension,
                      const char *order,
                      const char *label)
{
    ICSINIT;


    ICS_FM_WMD(ics);
    ICSTR(dimension >= ics->Dimensions, IcsErr_NotValidAction);
    if (order &&(order[0] != '\0')) {
        IcsStrCpy(ics->Dim[dimension].Order, order, ICS_STRLEN_TOKEN);
        if (label &&(label[0] != '\0')) {
            IcsStrCpy(ics->Dim[dimension].Label, label, ICS_STRLEN_TOKEN);
        } else {
            IcsStrCpy(ics->Dim[dimension].Label, order, ICS_STRLEN_TOKEN);
        }
    } else {
        if (label &&(label[0] != '\0')) {
            IcsStrCpy(ics->Dim[dimension].Label, label, ICS_STRLEN_TOKEN);
        } else {
            error = IcsErr_NotValidAction;
        }
    }

    return error;
}


/* Get the coordinate system used in the positioning of the pixels. Related to
   IcsGetPosition(). The default is "video". */
Ics_Error IcsGetCoordinateSystem(const ICS *ics,
                                 char      *coord)
{
    ICSINIT;


    ICS_FM_RMD(ics);
    ICSTR(coord == NULL, IcsErr_NotValidAction);
    if (ics->Coord[0] != '\0') {
        strcpy(coord, ics->Coord);
    } else {
        strcpy(coord, ICS_COORD_VIDEO);
    }

    return error;
}


/* Set the coordinate system used in the positioning of the pixels. Related to
   IcsSetPosition(). The default is "video". */
Ics_Error IcsSetCoordinateSystem(ICS        *ics,
                                 const char *coord)
{
    ICSINIT;


    ICS_FM_WMD(ics);
    if (coord &&(coord[0] != '\0')) {
        IcsStrCpy(ics->Coord, coord, ICS_STRLEN_TOKEN);
    } else {
        strcpy(ics->Coord, ICS_COORD_VIDEO);
    }

    return error;
}


/* Get the number of significant bits. */
Ics_Error IcsGetSignificantBits(const ICS *ics,
                                size_t    *nbits)
{
    ICSINIT;


    ICS_FM_RD(ics);
    ICSTR(nbits == NULL, IcsErr_NotValidAction);
    *nbits = ics->Imel.SigBits;

    return error;
}


/* Set the number of significant bits. */
Ics_Error IcsSetSignificantBits(ICS    *ics,
                                size_t  nbits)
{
    ICSINIT;
    size_t maxbits = IcsGetDataTypeSize(ics->Imel.DataType) * 8;


    ICS_FM_WD(ics);
    ICSTR(ics->Dimensions == 0, IcsErr_NoLayout);
    if (nbits > maxbits) {
        nbits = maxbits;
    }
    ics->Imel.SigBits = nbits;

    return error;
}


/* Set the position of the pixel values: the offset and scaling, and the units
   in which to measure. If you are not interested in one of the parameters, set
   the pointer to NULL. */
Ics_Error IcsGetImelUnits(const ICS *ics,
                          double    *origin,
                          double    *scale,
                          char      *units)
{
    ICSINIT;


    ICS_FM_RMD(ics);
    if (origin) {
        *origin = ics->Imel.Origin;
    }
    if (scale) {
        *scale = ics->Imel.Scale;
    }
    if (units) {
        if (ics->Imel.Unit[0] != '\0') {
            strcpy(units, ics->Imel.Unit);
        } else {
            strcpy(units, ICS_UNITS_RELATIVE);
        }
    }

    return error;
}


/* Set the position of the pixel values: the offset and scaling, and the units
   in which to measure. If units is NULL or empty, it is set to the default
   value of "relative". */
Ics_Error IcsSetImelUnits(ICS        *ics,
                          double      origin,
                          double      scale,
                          const char *units)
{
    ICSINIT;


    ICS_FM_WMD(ics);
    ics->Imel.Origin = origin;
    ics->Imel.Scale = scale;
    if (units &&(units[0] != '\0')) {
        IcsStrCpy(ics->Imel.Unit, units, ICS_STRLEN_TOKEN);
    } else {
        strcpy(ics->Imel.Unit, ICS_UNITS_RELATIVE);
    }

    return error;
}


/* Get the string for the SCIL_TYPE parameter. This string is used only by
   SCIL_Image. */
Ics_Error IcsGetScilType(const ICS *ics,
                         char      *sciltype)
{
    ICSINIT;


    ICS_FM_RMD(ics);
    ICSTR(sciltype == NULL, IcsErr_NotValidAction);
    strcpy(sciltype, ics->ScilType);

    return error;
}


/* Set the string for the SCIL_TYPE parameter. This string is used only by
   SCIL_Image. It is required if you want to read the image using SCIL_Image. */
Ics_Error IcsSetScilType(ICS        *ics,
                         const char *sciltype)
{
    ICSINIT;


    ICS_FM_WMD(ics);
    IcsStrCpy(ics->ScilType, sciltype, ICS_STRLEN_TOKEN);

    return error;
}


/* As IcsSetScilType, but creates a string according to the DataType in the ICS
   structure. It can create a string for g2d, g3d, f2d, f3d, c2d and c3d. */
Ics_Error IcsGuessScilType(ICS *ics)
{
    ICSINIT;


    ICS_FM_WMD(ics);
    switch (ics->Imel.DataType) {
        case Ics_uint8:
        case Ics_sint8:
        case Ics_uint16:
        case Ics_sint16:
            ics->ScilType[0] = 'g';
            break;
        case Ics_real32:
            ics->ScilType[0] = 'f';
            break;
        case Ics_complex32:
            ics->ScilType[0] = 'c';
            break;
        case Ics_uint32:
        case Ics_sint32:
        case Ics_real64:
        case Ics_complex64:
            return IcsErr_NoScilType;
        case Ics_unknown:
        default:
            ics->ScilType[0] = '\0';
            return IcsErr_NotValidAction;
    }
    if (ics->Dimensions == 3) {
        ics->ScilType[1] = '3';
    } else if (ics->Dimensions > 3) {
        ics->ScilType[0] = '\0';
        error = IcsErr_NoScilType;
    } else {
        ics->ScilType[1] = '2';
    }
    ics->ScilType[2] = 'd';
    ics->ScilType[3] = '\0';

    return error;
}


/* Returns a textual description of the error code. */
const char *IcsGetErrorText(Ics_Error error)
{
    const char *msg;

    switch (error) {
        case IcsErr_Ok:
            msg = "A-OK";
            break;
        case IcsErr_FSizeConflict:
            msg = "Non fatal error: unexpected data size";
            break;
        case IcsErr_OutputNotFilled:
            msg = "Non fatal error: the output buffer could not be completely "
                "filled";
            break;
        case IcsErr_Alloc:
            msg = "Memory allocation error";
            break;
        case IcsErr_BitsVsSizeConfl:
            msg = "Image size conflicts with bits per element";
            break;
        case IcsErr_BlockNotAllowed:
            msg = "It is not possible to read COMPRESS-compressed data in "
                "blocks";
            break;
        case IcsErr_BufferTooSmall:
            msg = "The buffer was too small to hold the given ROI";
            break;
        case IcsErr_CompressionProblem:
            msg = "Some error occurred during compression";
            break;
        case IcsErr_CorruptedStream:
            msg = "The compressed input stream is currupted";
            break;
        case IcsErr_DecompressionProblem:
            msg = "Some error occurred during decompression";
            break;
        case IcsErr_DuplicateData:
            msg = "The ICS data structure already contains incompatible stuff";
            break;
        case IcsErr_EmptyField:
            msg = "Empty field";
            break;
        case IcsErr_EndOfHistory:
            msg = "All history lines have already been returned";
            break;
        case IcsErr_EndOfStream:
            msg = "Unexpected end of stream";
            break;
        case IcsErr_FailWriteLine:
            msg = "Failed to write a line in .ics file";
            break;
        case IcsErr_FCloseIcs:
            msg = "File close error on .ics file";
            break;
        case IcsErr_FCloseIds:
            msg = "File close error on .ids file";
            break;
        case IcsErr_FCopyIds:
            msg = "Failed to copy image data from temporary file on .ics file "
                "opened for updating";
            break;
        case IcsErr_FOpenIcs:
            msg = "File open error on .ics file";
            break;
        case IcsErr_FOpenIds:
            msg = "File open error on .ids file";
            break;
        case IcsErr_FReadIcs:
            msg = "File read error on .ics file";
            break;
        case IcsErr_FReadIds:
            msg = "File read error on .ids file";
            break;
        case IcsErr_FTempMoveIcs:
            msg = "Failed to remane .ics file opened for updating";
            break;
        case IcsErr_FWriteIcs:
            msg = "File write error on .ics file";
            break;
        case IcsErr_FWriteIds:
            msg = "File write error on .ids file";
            break;
        case IcsErr_IllegalROI:
            msg = "The given ROI extends outside the image";
            break;
        case IcsErr_IllIcsToken:
            msg = "Illegal ICS token detected";
            break;
        case IcsErr_IllParameter:
            msg = "A function parameter has a value that is not legal or does "
                "not match with a value previously given";
            break;
        case IcsErr_LineOverflow:
            msg = "Line overflow in .ics file";
            break;
        case IcsErr_MissBits:
            msg = "Missing \"bits\" element in .ics file";
            break;
        case IcsErr_MissCat:
            msg = "Missing main category";
            break;
        case IcsErr_MissingData:
            msg = "There is no Data defined";
            break;
        case IcsErr_MissLayoutSubCat:
            msg = "Missing layout subcategory";
            break;
        case IcsErr_MissParamSubCat:
            msg = "Missing parameter subcategory";
            break;
        case IcsErr_MissRepresSubCat:
            msg = "Missing representation subcategory";
            break;
        case IcsErr_MissSensorSubCat:
            msg = "Missing sensor subcategory";
            break;
        case IcsErr_MissSensorSubSubCat:
            msg = "Missing sensor subsubcategory";
            break;
        case IcsErr_MissSubCat:
            msg = "Missing sub category";
            break;
        case IcsErr_NoLayout:
            msg = "Layout parameters missing or not defined";
            break;
        case IcsErr_NoScilType:
            msg = "There doesn't exist a SCIL_TYPE value for this image";
            break;
        case IcsErr_NotIcsFile:
            msg = "Not an ICS file";
            break;
        case IcsErr_NotValidAction:
            msg = "The function won't work on the ICS given";
            break;
        case IcsErr_TooManyChans:
            msg = "Too many channels specified";
            break;
        case IcsErr_TooManyDims:
            msg = "Data has too many dimensions";
            break;
        case IcsErr_UnknownCompression:
            msg = "Unknown compression type";
            break;
        case IcsErr_UnknownDataType:
            msg = "The datatype is not recognized";
            break;
        case IcsErr_WrongZlibVersion:
            msg = "libics is linking to a different version of zlib than used "
                "during compilation";
            break;
        default:
            msg = "Some error occurred I know nothing about.";
    }
    return msg;
}
