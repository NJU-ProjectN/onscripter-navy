/* -*- C++ -*-
 *
 *  ONScripter_file.cpp - FILE I/O of ONScripter
 *
 *  Copyright (c) 2001-2018 Ogapee. All rights reserved.
 *            (C) 2014-2018 jh10001 <jh10001@live.cn>
 *
 *  ogapee@aqua.dti2.ne.jp
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "ONScripter.h"
#include "Utils.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#define SAVEFILE_MAGIC_NUMBER "ONS"
#define SAVEFILE_VERSION_MAJOR 2
#define SAVEFILE_VERSION_MINOR 8

#define READ_LENGTH 4096

void ONScripter::searchSaveFile( SaveFileInfo &save_file_info, int no )
{
    char file_name[256];

    script_h.getStringFromInteger( save_file_info.sjis_no, no, (num_save_file >= 10)?2:1 );
    sprintf( file_name, "%ssave%d.dat", save_dir?save_dir:archive_path, no );
    time_t mtime = 0;
#ifdef __NAVY__
    FILE *fp = ::fopen(file_name, "r");
    if (fp == NULL) {
      save_file_info.valid = false;
      return;
    }
    fclose(fp);
#else
    struct stat buf;
    if ( stat( file_name, &buf ) != 0 ){
        save_file_info.valid = false;
        return;
    }
    mtime = buf.st_mtime;
#endif
    struct tm *tm = localtime( &mtime );
        
    save_file_info.month  = tm->tm_mon + 1;
    save_file_info.day    = tm->tm_mday;
    save_file_info.hour   = tm->tm_hour;
    save_file_info.minute = tm->tm_min;
    save_file_info.valid = true;
    script_h.getStringFromInteger( save_file_info.sjis_month,  save_file_info.month,  2 );
    script_h.getStringFromInteger( save_file_info.sjis_day,    save_file_info.day,    2 );
    script_h.getStringFromInteger( save_file_info.sjis_hour,   save_file_info.hour,   2 );
    script_h.getStringFromInteger( save_file_info.sjis_minute, save_file_info.minute, 2, true );
}

char *ONScripter::readSaveStrFromFile( int no )
{
    char filename[32];
    sprintf( filename, "save%d.dat", no );
    size_t len = loadFileIOBuf( filename );
    if (len == 0){
        utils::printError("readSaveStrFromFile: can't open save file %s\n", filename );
        return NULL;
    }

    int p = len - 1;
    if ( p < 3 || file_io_buf[p] != '*' || file_io_buf[p-1] != '"' ) return NULL;
    p -= 2;
    
    while( file_io_buf[p] != '"' && p>0 ) p--;
    if ( file_io_buf[p] != '"' ) return NULL;

    len = len - p - 3;
    char *buf = new char[len+1];
    
    unsigned int i;
    for (i=0 ; i<len ; i++)
        buf[i] = file_io_buf[p+i+1];
    buf[i] = 0;

    return buf;
}

int ONScripter::loadSaveFile( int no )
{
    char filename[32];
    sprintf( filename, "save%d.dat", no );
    if (loadFileIOBuf( filename ) == 0){
        utils::printError("can't open save file %s\n", filename );
        return -1;
    }
    
    /* ---------------------------------------- */
    /* Load magic number */
    int i;
    for ( i=0 ; i<(int)strlen( SAVEFILE_MAGIC_NUMBER ) ; i++ )
        if ( readChar() != SAVEFILE_MAGIC_NUMBER[i] ) break;

    if ( i != (int)strlen( SAVEFILE_MAGIC_NUMBER ) ){
        file_io_buf_ptr = 0;
        printf("Save file version is unknown\n" );
        return loadSaveFile2( SAVEFILE_VERSION_MAJOR*100 + SAVEFILE_VERSION_MINOR );
    }
    
    int file_version = readChar() * 100;
    file_version += readChar();
    printf("Save file version is %d.%d\n", file_version/100, file_version%100 );
    if ( file_version > SAVEFILE_VERSION_MAJOR*100 + SAVEFILE_VERSION_MINOR ){
        utils::printError("Save file is newer than %d.%d, please use the latest ONScripter.\n", SAVEFILE_VERSION_MAJOR, SAVEFILE_VERSION_MINOR );
        return -1;
    }

    if ( file_version >= 200 )
        return loadSaveFile2( file_version );
    
    utils::printError("Save file is too old.\n");

    return -1;
}

void ONScripter::saveMagicNumber( bool output_flag )
{
    for ( unsigned int i=0 ; i<strlen( SAVEFILE_MAGIC_NUMBER ) ; i++ )
        writeChar( SAVEFILE_MAGIC_NUMBER[i], output_flag );
    writeChar( SAVEFILE_VERSION_MAJOR, output_flag );
    writeChar( SAVEFILE_VERSION_MINOR, output_flag );
}

void ONScripter::storeSaveFile()
{
    file_io_buf_ptr = 0;
    saveMagicNumber( false );
    saveSaveFile2( false );
    allocFileIOBuf();
    saveMagicNumber( true );
    saveSaveFile2( true );
    save_data_len = file_io_buf_ptr;
    memcpy(save_data_buf, file_io_buf, save_data_len);
}

int ONScripter::writeSaveFile( int no, const char *savestr )
{
    saveAll();

    char filename[32];
    sprintf( filename, "save%d.dat", no );
        
    memcpy(file_io_buf, save_data_buf, save_data_len);
    file_io_buf_ptr = save_data_len;
    if (saveFileIOBuf( filename, 0, savestr )){
        utils::printError("can't open save file %s for writing\n", filename );
        return -1;
    }

    size_t magic_len = strlen(SAVEFILE_MAGIC_NUMBER)+2;
    sprintf( filename, RELATIVEPATH "sav%csave%d.dat", DELIMITER, no );
    if (saveFileIOBuf( filename, magic_len, savestr ))
        utils::printError("can't open save file %s for writing (not an error)\n", filename );

    return 0;
}
