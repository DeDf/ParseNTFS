
#include "ParseNtfs.h"

BOOL
FixMultiSector(__inout PMULTI_SECTOR_HEADER pMultiSector,
              __in DWORD WordsPerSector)
{
    PUSHORT USA = (PUSHORT)(PUCHAR(pMultiSector) +
        pMultiSector->UpdateSequenceArrayOffset);
    ULONG UsaCount = pMultiSector->UpdateSequenceArraySize;
    PUSHORT Sector = (PUSHORT)pMultiSector;

    printf("UsaCount = %d\n", UsaCount);

    for (ULONG Index = 1; Index < UsaCount; ++Index)
    {
        if (Sector[WordsPerSector - 1] == USA[0])
        {
            Sector[WordsPerSector - 1] = USA[Index];
            Sector += WordsPerSector;
        }
        else
        {
            printf("USA error at 0x%p\n", Sector + WordsPerSector - 1);
            return FALSE;
        }
    }

    return TRUE;
}


void Parse$DATA(ULONG Cluster, HANDLE FileHandle)
{
    UCHAR buf[0x1200];
    PINDEX_ALLOCATION_BUFFER p = (PINDEX_ALLOCATION_BUFFER)buf;
    ULONG ulReturn;
    //
    LARGE_INTEGER lpNewFilePointer;
    lpNewFilePointer.QuadPart = Cluster*8*0x200;

    printf("     ParseA0 Cluster : 0x%x\n", Cluster);
    SetFilePointerEx(FileHandle,lpNewFilePointer,NULL,FILE_BEGIN);
    ReadFile(FileHandle,p,sizeof(buf),&ulReturn,NULL);

    HANDLE hFile = CreateFileA("d:\\1.1", // FileName
        GENERIC_READ | GENERIC_WRITE,     // open for reading
        FILE_SHARE_READ,                  // share to read
        NULL,                             // no security
        CREATE_NEW,                       // existing file only
        0,
        NULL);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        printf("Could not CreateFile.\n");
        return;
    }

    ULONG dwBytesRead;
    WriteFile(hFile, buf, sizeof(buf), &dwBytesRead, NULL);
    CloseHandle(hFile);
}

void ParseA0(ULONG Cluster, HANDLE FileHandle)
{
    UCHAR buf[0x1000];
    PINDEX_ALLOCATION_BUFFER p = (PINDEX_ALLOCATION_BUFFER)buf;
    PINDEX_ENTRY pIndexEntry;
    ULONG ulReturn;
    //
    LARGE_INTEGER lpNewFilePointer;
    lpNewFilePointer.QuadPart = Cluster*8*0x200;

    printf("     ParseA0 Cluster : 0x%x\n", Cluster);
    SetFilePointerEx(FileHandle,lpNewFilePointer,NULL,FILE_BEGIN);
    ReadFile(FileHandle,p,sizeof(buf),&ulReturn,NULL);

    FixMultiSector((PMULTI_SECTOR_HEADER)p, 256);

    pIndexEntry = (PINDEX_ENTRY)
        ( (PUCHAR)&p->IndexHeader + p->IndexHeader.FirstIndexEntry );

    while ( pIndexEntry->Flags == 0 ||
        pIndexEntry->Flags == INDEX_ENTRY_NODE )
    {
        if ( pIndexEntry->FileName.Flags & FILE_NAME_NTFS )
        {
            if (pIndexEntry->FileName.FileNameLength &&
                pIndexEntry->FileName.FileNameLength < MAX_PATH )
            {
                ULONG64 FRSNumber = 0; 
                WCHAR FileName[MAX_PATH];
                memcpy(FileName, pIndexEntry->FileName.FileName, pIndexEntry->FileName.FileNameLength*2);
                FileName[pIndexEntry->FileName.FileNameLength] = 0;

                FRSNumber |= pIndexEntry->FileReference.SegmentNumberHighPart << 32;
                FRSNumber |= pIndexEntry->FileReference.SegmentNumberLowPart;
                printf( "        File Name: %-32ws %10I64d\n", FileName, FRSNumber);
            }
        }

        pIndexEntry = (PINDEX_ENTRY)((ULONG)pIndexEntry + pIndexEntry->Length );
    }

    ulReturn = 0;  // for Debug BreakPoint
}

void AnalyseAttribute(PATTRIBUTE_RECORD_HEADER pAttribute, HANDLE FileHandle)
{
    switch(pAttribute->TypeCode)
    {
    case $STANDARD_INFORMATION:
        printf(" AttrType: $STANDARD_INFORMATION\n");
        break;

    case $ATTRIBUTE_LIST:
        printf(" AttrType: $ATTRIBUTE_LIST\n");
        break;

    case $FILE_NAME:
        {
            printf(" AttrType: $FILE_NAME");

            if ( pAttribute->FormCode == RESIDENT_FORM )
            {
                PFILE_NAME  pFileName = (PFILE_NAME)
                    ((ULONG)pAttribute + pAttribute->Form.Resident.ValueOffset);

                if ( pFileName->Flags & FILE_NAME_NTFS )
                {
                    if (pFileName->FileNameLength < MAX_PATH )
                    {
                        WCHAR FileName[MAX_PATH];
                        memcpy(FileName, pFileName->FileName, pFileName->FileNameLength*2);
                        FileName[pFileName->FileNameLength] = 0;
                        printf( " : %ws\n", FileName);
                        break;
                    }
                }
            }
            printf("\n");
            break;
        }

    case $OBJECT_ID:
        printf(" AttrType: $OBJECT_ID\n");
        break;
    case $SECURITY_DESCRIPTOR:
        printf(" AttrType: $SECURITY_DESCRIPTOR\n");
        break;
    case $VOLUME_NAME:
        printf(" AttrType: $VOLUME_NAME: ");
        {
            if ( pAttribute->FormCode == RESIDENT_FORM )
            {
                if (pAttribute->Form.Resident.ValueLength &&
                    pAttribute->Form.Resident.ValueLength < MAX_PATH*2)
                {
                    WCHAR VolumeName[MAX_PATH];
                    memcpy(VolumeName,
                        PVOID(ULONG(pAttribute) + pAttribute->Form.Resident.ValueOffset),
                        pAttribute->Form.Resident.ValueLength );

                    VolumeName[pAttribute->Form.Resident.ValueLength/2] = 0;
                    printf( "%ws\n", VolumeName);
                }
                else
                    printf("本地磁盘\n");
            }
        }
        break;
    case $VOLUME_INFORMATION:
        printf(" AttrType: $VOLUME_INFORMATION: ");

        if ( pAttribute->FormCode == RESIDENT_FORM )
        {
            PVOLUME_INFORMATION  pVolumeInfo = (PVOLUME_INFORMATION)
                ((ULONG)pAttribute + pAttribute->Form.Resident.ValueOffset);

            printf( "NTFS_ver: %d.%d\n",
                pVolumeInfo->MajorVersion,
                pVolumeInfo->MinorVersion );
        }

        break;
    case $DATA:
        printf(" AttrType: $DATA\n");
        {
            int i = 0;
        
        if (i)
        if (pAttribute->FormCode == NONRESIDENT_FORM)
        {
            PUCHAR p = (PUCHAR)pAttribute + pAttribute->Form.Nonresident.MappingPairsOffset;

            ULONG subtractor;  // 如果头一个字节首bit为1，说明是负数的补码，需要减去这个

            ULONG Cluster = 0;

            while (*p & 0xF0)
            {
                UCHAR nOffsetLen  = (*p & 0xF0)>>4;
                UCHAR tOffsetLen  = nOffsetLen;
                UCHAR nClusterLen = *(p+1);
                ULONG t = 0;
                subtractor = 0;

                while(tOffsetLen)
                {
                    UCHAR c = *(p+2+tOffsetLen-1);

                    t |= c;

                    if (tOffsetLen == nOffsetLen)
                    {
                        if (c & 0x80)
                            subtractor = 1 << (nOffsetLen*8);
                    }

                    if (--tOffsetLen)
                        t <<= 8;
                }

                printf("       ClusterOffset : 0x%x", t);  // 这里单位是簇

                if (subtractor)
                    printf("  subtractor : 0x%x\n", subtractor);
                else
                    printf("\n");

                if (Cluster)
                    Cluster = Cluster + t - subtractor;
                else
                    Cluster = t;

                Parse$DATA(Cluster, FileHandle);

                p += nOffsetLen+2;
            }
        }
        }
        break;

    case $INDEX_ROOT:
        {
            printf(" AttrType: $INDEX_ROOT\n");

            PINDEX_ROOT  pIndexRoot = (PINDEX_ROOT)
                ((ULONG)pAttribute + pAttribute->Form.Resident.ValueOffset);

            PINDEX_HEADER pIndexHeader = &pIndexRoot->IndexHeader;
            //定位到索引项
            PINDEX_ENTRY pIndexEntry = (PINDEX_ENTRY)
                ((ULONG)pIndexHeader + pIndexRoot->IndexHeader.FirstIndexEntry);

            while ( pIndexEntry->Flags == 0 ||
                    pIndexEntry->Flags == INDEX_ENTRY_NODE )
            {
                if ( pIndexEntry->FileName.Flags & FILE_NAME_NTFS )
                {
                    if (pIndexEntry->FileName.FileNameLength &&
                        pIndexEntry->FileName.FileNameLength < MAX_PATH )
                    {
                        WCHAR FileName[MAX_PATH];
                        memcpy(FileName, pIndexEntry->FileName.FileName, pIndexEntry->FileName.FileNameLength*2);
                        FileName[pIndexEntry->FileName.FileNameLength] = 0;
                        printf( "        File Name: %ws\n", FileName);
                    }
                }

                pIndexEntry = (PINDEX_ENTRY)((ULONG)pIndexEntry + pIndexEntry->Length );
            }

            break;
        }

    case $INDEX_ALLOCATION:
        printf(" AttrType: $INDEX_ALLOCATION\n");
        if (pAttribute->FormCode == NONRESIDENT_FORM)
        {
            PUCHAR p = (PUCHAR)pAttribute + pAttribute->Form.Nonresident.MappingPairsOffset;

            ULONG subtractor;  // 如果头一个字节首bit为1，说明是负数的补码，需要减去这个
            
            ULONG Cluster = 0;

            while (*p & 0xF0)
            {
                UCHAR nOffsetLen  = (*p & 0xF0)>>4;
                UCHAR tOffsetLen  = nOffsetLen;
                UCHAR nClusterSum = *(p+1);
                ULONG t = 0;
                subtractor = 0;

                while(tOffsetLen)
                {
                    UCHAR c = *(p+2+tOffsetLen-1);

                    t |= c;

                    if (tOffsetLen == nOffsetLen)
                    {
                        if (c & 0x80)
                            subtractor = 1 << (nOffsetLen*8);
                    }

                    if (--tOffsetLen)
                        t <<= 8;
                }

                printf("       ClusterOffset : 0x%x, nClusterSum : %d", t, nClusterSum);  // 这里单位是簇

                if (subtractor)
                    printf("  subtractor : 0x%x\n", subtractor);
                else
                    printf("\n");

                if (Cluster)
                    Cluster = Cluster + t - subtractor;
                else
                    Cluster = t;

                ParseA0(Cluster, FileHandle);

                p += nOffsetLen+2;
            }
        }
        break;

    case $BITMAP:
        printf(" AttrType: $BITMAP\n");
        break;

    case $SYMBOLIC_LINK:
        printf(" AttrType: $BITMAP\n");
        break;
    }
}

void AnalysePartition()
{
    HANDLE FileHandle = CreateFileW(
        L"\\\\.\\C:",
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        0,
        OPEN_EXISTING,
        0,
        0);

    UCHAR buf[0x400];
    ULONG ulReturn;
    ReadFile(FileHandle,buf,0x200,&ulReturn,NULL);

    PPACKED_BOOT_SECTOR pBootSector = (PPACKED_BOOT_SECTOR)buf;

	LARGE_INTEGER ulStartSector;

	ulStartSector.QuadPart =
        pBootSector->MftStartLcn * pBootSector->PackedBpb.SectorsPerCluster;

    LARGE_INTEGER lpNewFilePointer;
    lpNewFilePointer.QuadPart = ulStartSector.QuadPart*0x200;  // 定位到MFT的起始地址
    LARGE_INTEGER MFTStart;
    MFTStart.QuadPart = lpNewFilePointer.QuadPart;
    printf("MFTStart : 0x%I64x\n", MFTStart.QuadPart);

    for (int i = 0; i < 16; i++, lpNewFilePointer.QuadPart += 0x400)  // 一个MFT记录是1024(0x400)字节
    {
        printf("FRS %d\n", i);  // file record segment

        SetFilePointerEx(FileHandle,lpNewFilePointer,NULL,FILE_BEGIN);
        ReadFile(FileHandle,buf,0x400,&ulReturn,NULL);

        PFILE_RECORD_SEGMENT_HEADER pFileRecord = (PFILE_RECORD_SEGMENT_HEADER)buf;

        if (*(ULONG*)pFileRecord != 'ELIF')
        {
            printf("invalid file record!\n");
            goto Exit;
        }
        FixMultiSector((PMULTI_SECTOR_HEADER)pFileRecord, 256);

        PATTRIBUTE_RECORD_HEADER pAttribute;

        for(pAttribute = (PATTRIBUTE_RECORD_HEADER) ((ULONG)pFileRecord + pFileRecord->FirstAttributeOffset);
            pAttribute->TypeCode != $END;
            pAttribute = (PATTRIBUTE_RECORD_HEADER) ((ULONG)pAttribute + pAttribute->RecordLength))
        {
            AnalyseAttribute(pAttribute, FileHandle);
        } 
    }

    PFILE_RECORD_SEGMENT_HEADER pFileRecord = (PFILE_RECORD_SEGMENT_HEADER)buf;
    PATTRIBUTE_RECORD_HEADER pAttribute;
    ULONG FileMFTNumber;

    printf ("============================================\n");
    FileMFTNumber = 4961;
    printf ("FRS %d\n", FileMFTNumber);
    lpNewFilePointer.QuadPart = MFTStart.QuadPart + FileMFTNumber * 0x400;
    
    SetFilePointerEx(FileHandle,lpNewFilePointer,NULL,FILE_BEGIN);
    ReadFile(FileHandle,buf,0x400,&ulReturn,NULL);
    
    if (*(ULONG*)pFileRecord != 'ELIF')
    {
        printf("invalid file record!\n");
        goto Exit;
    }
    FixMultiSector((PMULTI_SECTOR_HEADER)pFileRecord, 256);

    for(pAttribute = (PATTRIBUTE_RECORD_HEADER) ((ULONG)pFileRecord + pFileRecord->FirstAttributeOffset);
        pAttribute->TypeCode != $END;
        pAttribute = (PATTRIBUTE_RECORD_HEADER) ((ULONG)pAttribute + pAttribute->RecordLength))
    {
        AnalyseAttribute(pAttribute, FileHandle);
    }

    printf ("============================================\n");
    FileMFTNumber = 29862;
    printf ("FRS %d\n", FileMFTNumber);
    lpNewFilePointer.QuadPart = MFTStart.QuadPart + FileMFTNumber * 0x400;

    SetFilePointerEx(FileHandle,lpNewFilePointer,NULL,FILE_BEGIN);
    ReadFile(FileHandle,buf,0x400,&ulReturn,NULL);

    if (*(ULONG*)pFileRecord != 'ELIF')
    {
        printf("invalid file record!\n");
        goto Exit;
    }
    FixMultiSector((PMULTI_SECTOR_HEADER)pFileRecord, 256);

    for(pAttribute = (PATTRIBUTE_RECORD_HEADER) ((ULONG)pFileRecord + pFileRecord->FirstAttributeOffset);
        pAttribute->TypeCode != $END;
        pAttribute = (PATTRIBUTE_RECORD_HEADER) ((ULONG)pAttribute + pAttribute->RecordLength))
    {
        AnalyseAttribute(pAttribute, FileHandle);
    }

Exit:
    if (FileHandle)
        CloseHandle(FileHandle);
}

