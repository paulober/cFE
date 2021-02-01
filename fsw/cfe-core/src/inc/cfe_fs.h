/*
**  GSC-18128-1, "Core Flight Executive Version 6.7"
**
**  Copyright (c) 2006-2019 United States Government as represented by
**  the Administrator of the National Aeronautics and Space Administration.
**  All Rights Reserved.
**
**  Licensed under the Apache License, Version 2.0 (the "License");
**  you may not use this file except in compliance with the License.
**  You may obtain a copy of the License at
**
**    http://www.apache.org/licenses/LICENSE-2.0
**
**  Unless required by applicable law or agreed to in writing, software
**  distributed under the License is distributed on an "AS IS" BASIS,
**  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
**  See the License for the specific language governing permissions and
**  limitations under the License.
*/

/*
** File: cfe_fs.h
**
** Purpose:  cFE File Services (FS) library API header file
**
** Author:   S.Walling/Microtel
**
*/

/*
** Ensure that header is included only once...
*/
#ifndef _cfe_fs_
#define _cfe_fs_


/*
** Required header files...
*/
#include "cfe_fs_extern_typedefs.h"
#include "cfe_error.h"
#include "common_types.h"
#include "cfe_time.h"

/*
 * Because FS is a library not an app, it does not have its own context or
 * event IDs.  The file writer runs in the context of the ES background task
 * on behalf of whatever App requested the file write.
 * 
 * This is a list of abstract events associated with background file write jobs.
 * An app requesting the file write must supply a callback function to translate
 * these into its own event IDs for feedback (i.e. file complete, error conditions, etc).
 */
typedef enum
{
    CFE_FS_FileWriteEvent_UNDEFINED, /* placeholder, no-op, keep as 0 */

    CFE_FS_FileWriteEvent_COMPLETE,             /**< File is completed successfully */
    CFE_FS_FileWriteEvent_CREATE_ERROR,         /**< Unable to create/open file */
    CFE_FS_FileWriteEvent_HEADER_WRITE_ERROR,   /**< Unable to write FS header */
    CFE_FS_FileWriteEvent_RECORD_WRITE_ERROR,   /**< Unable to write data record */

    CFE_FS_FileWriteEvent_MAX /* placeholder, no-op, keep last */

} CFE_FS_FileWriteEvent_t;


/**
 * Data Getter routine provided by requester
 * 
 * Outputs a data block.  Should return true if the file is complete (last record/EOF), otherwise return false.
 */
typedef bool (*CFE_FS_FileWriteGetData_t)(void *Meta, uint32 RecordNum, void **Buffer, size_t *BufSize);

/**
 * Event generator routine provided by requester
 * 
 * Invoked from certain points in the file write process.  Implementation may invoke CFE_EVS_SendEvent() appropriately
 * to inform of progress.
 */
typedef void (*CFE_FS_FileWriteOnEvent_t)(void *Meta, CFE_FS_FileWriteEvent_t Event, int32 Status, uint32 RecordNum, size_t BlockSize, size_t Position);

/**
 * \brief External Metadata/State object associated with background file writes
 *
 * Applications intending to schedule background file write jobs should instantiate
 * this object in static/global data memory.  This keeps track of the state of the
 * file write request(s).
 */ 
typedef struct CFE_FS_FileWriteMetaData
{
    volatile bool IsPending;            /**< Whether request is pending (volatile as it may be checked outside lock) */

    char   FileName[OS_MAX_PATH_LEN];   /**< Name of file to write */

    /* Data for FS header */
    uint32 FileSubType;                          /**< Type of file to write (for FS header) */
    char   Description[CFE_FS_HDR_DESC_MAX_LEN]; /**< Description of file (for FS header) */

    CFE_FS_FileWriteGetData_t GetData;  /**< Application callback to get a data record */
    CFE_FS_FileWriteOnEvent_t OnEvent;  /**< Application callback for abstract event processing */

} CFE_FS_FileWriteMetaData_t;


/** @defgroup CFEAPIFSHeader cFE File Header Management APIs
 * @{
 */

/*****************************************************************************/
/**
** \brief Read the contents of the Standard cFE File Header
**
** \par Description
**        This API will fill the specified #CFE_FS_Header_t variable with the
**        contents of the Standard cFE File Header of the file identified by
**        the given File Descriptor.
**
** \par Assumptions, External Events, and Notes:
**        -# The File has already been successfully opened using #OS_OpenCreate and
**           the caller has a legitimate File Descriptor.
**
** \param[in] FileDes File Descriptor obtained from a previous call to #OS_OpenCreate
**                    that is associated with the file whose header is to be read.
**
** \param[in, out] Hdr     Pointer to a variable of type #CFE_FS_Header_t that will be
**                    filled with the contents of the Standard cFE File Header. *Hdr is the contents of the Standard cFE File Header for the specified file.
**
** \return Execution status, see \ref CFEReturnCodes
**
** \sa #CFE_FS_WriteHeader
**
******************************************************************************/
CFE_Status_t CFE_FS_ReadHeader(CFE_FS_Header_t *Hdr, osal_id_t FileDes);

/*****************************************************************************/
/**
** \brief Initializes the contents of the Standard cFE File Header
**
** \par Description
**        This API will clear the specified #CFE_FS_Header_t variable and
**        initialize the description field with the specified value
**
** \param[in] Hdr          Pointer to a variable of type #CFE_FS_Header_t that will be
**                         cleared and initialized
**
** \param[in] *Description Initializes Header's Description
**
** \param[in]  SubType     Initializes Header's SubType
**
** \sa #CFE_FS_WriteHeader
**
******************************************************************************/
void CFE_FS_InitHeader(CFE_FS_Header_t *Hdr, const char *Description, uint32 SubType);

/*****************************************************************************/
/**
** \brief Write the specified Standard cFE File Header to the specified file
**
** \par Description
**        This API will output the specified #CFE_FS_Header_t variable, with some
**        fields automatically updated, to the specified file as the Standard cFE
**        File Header. This API will automatically populate the following fields
**        in the specified #CFE_FS_Header_t:
**
**      -# \link #CFE_FS_Header_t::ContentType \c ContentType \endlink - Filled with 0x63464531 ('cFE1')
**      -# \link #CFE_FS_Header_t::Length \c Length \endlink - Filled with the sizeof(CFE_FS_Header_t)
**      -# \link #CFE_FS_Header_t::SpacecraftID \c SpacecraftID \endlink - Filled with the Spacecraft ID
**      -# \link #CFE_FS_Header_t::ProcessorID \c ProcessorID \endlink - Filled with the Processor ID
**      -# \link #CFE_FS_Header_t::ApplicationID \c ApplicationID \endlink -  Filled with the Application ID
**      -# \link #CFE_FS_Header_t::TimeSeconds \c TimeSeconds \endlink - Filled with the Time, in seconds, as obtained by #CFE_TIME_GetTime
**      -# \link #CFE_FS_Header_t::TimeSubSeconds \c TimeSubSeconds \endlink - Filled with the Time, subseconds, as obtained by #CFE_TIME_GetTime
**        
**        
** \par Assumptions, External Events, and Notes:
**        -# The File has already been successfully opened using #OS_OpenCreate and
**           the caller has a legitimate File Descriptor.
**        -# The \c SubType field has been filled appropriately by the Application.
**        -# The \c Description field has been filled appropriately by the Application.
**
** \param[in] FileDes File Descriptor obtained from a previous call to #OS_OpenCreate
**                    that is associated with the file whose header is to be read.
**
** \param[in, out] Hdr     Pointer to a variable of type #CFE_FS_Header_t that will be
**                    filled with the contents of the Standard cFE File Header. *Hdr is the contents of the Standard cFE File Header for the specified file.
**
** \return Execution status, see \ref CFEReturnCodes
**
** \sa #CFE_FS_ReadHeader
**
******************************************************************************/
CFE_Status_t CFE_FS_WriteHeader(osal_id_t FileDes, CFE_FS_Header_t *Hdr);

/*****************************************************************************/
/**
** \brief Modifies the Time Stamp field in the Standard cFE File Header for the specified file
**
** \par Description
**        This API will modify the \link #CFE_FS_Header_t::TimeSeconds timestamp \endlink found
**        in the Standard cFE File Header of the specified file.  The timestamp will be replaced
**        with the time specified by the caller.
**
** \par Assumptions, External Events, and Notes:
**        -# The File has already been successfully opened using #OS_OpenCreate and
**           the caller has a legitimate File Descriptor.
**        -# The \c NewTimestamp field has been filled appropriately by the Application.
**
** \param[in] FileDes File Descriptor obtained from a previous call to #OS_OpenCreate
**                    that is associated with the file whose header is to be read.
**
** \param[in] NewTimestamp A #CFE_TIME_SysTime_t data structure containing the desired time
**                         to be put into the file's Standard cFE File Header.
**
** \return Execution status, see \ref CFEReturnCodes
**               
******************************************************************************/
CFE_Status_t CFE_FS_SetTimestamp(osal_id_t FileDes, CFE_TIME_SysTime_t NewTimestamp);
/**@}*/


/** @defgroup CFEAPIFSUtil cFE File Utility APIs
 * @{
 */

/*****************************************************************************/
/**
** \brief Extracts the filename from a unix style path and filename string.
**
** \par Description
**        This API will take the original unix path/filename combination and
**        extract the base filename. Example: Given the path/filename : "/cf/apps/myapp.o.gz"
**        this function will return the filename: "myapp.o.gz".
**
** \par Assumptions, External Events, and Notes:
**        -# The paths and filenames used here are the standard unix style
**            filenames separated by "/" characters.
**        -# The extracted filename (including terminator) is no longer than #OS_MAX_PATH_LEN 
**
** \param[in] OriginalPath The original path.
** \param[out] FileNameOnly The filename that is extracted from the path.
**
** \return Execution status, see \ref CFEReturnCodes
**
******************************************************************************/
CFE_Status_t CFE_FS_ExtractFilenameFromPath(const char *OriginalPath, char *FileNameOnly);

/*****************************************************************************/
/**
** \brief Register a background file dump request
**
** \par Description
**        Puts the previously-initialized metadata into the pending request queue
**
** \par Assumptions, External Events, and Notes:
**        Metadata structure should be stored in a static memory area (not on heap) as it
**        must persist and be accessible by the file writer task throughout the asynchronous
**        job operation.
**
** \param[inout] Meta        The background file write persistent state object
**
** \return Execution status, see \ref CFEReturnCodes
**
******************************************************************************/
int32 CFE_FS_BackgroundFileDumpRequest(CFE_FS_FileWriteMetaData_t *Meta);

/*****************************************************************************/
/**
** \brief Query if a background file write request is currently pending
**
** \par Description
**        This returns "true" while the request is on the background work queue
**        This returns "false" once the request is complete and removed from the queue.
**
** \par Assumptions, External Events, and Notes:
**        None
**
** \param[inout] Meta        The background file write persistent state object
**
** \return true if request is already pending, false if not
**
******************************************************************************/
bool CFE_FS_BackgroundFileDumpIsPending(const CFE_FS_FileWriteMetaData_t *Meta);

/*****************************************************************************/
/**
** \brief Execute the background file write job(s)
**
** \par Description
**        Runs the state machine associated with background file write requests
**
** \par Assumptions, External Events, and Notes:
**        This should only be invoked as a background job from the ES background task,
**        it should not be invoked directly.
**
** \param[in] ElapsedTime       The amount of time passed since last invocation (ms)
** \param[in] Arg               Not used/ignored
**
** \return true if jobs are pending, false if idle
**
******************************************************************************/
bool CFE_FS_RunBackgroundFileDump(uint32 ElapsedTime, void *Arg);

/**@}*/

#endif /* _cfe_fs_ */

/************************/
/*  End of File Comment */
/************************/
