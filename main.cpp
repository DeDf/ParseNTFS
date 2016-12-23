
#include "ParseNtfs.h"

void AnalyseMBR()
{
    HANDLE FileHandle = CreateFileW(
        L"\\\\.\\PhysicalDrive0",
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        0,
        OPEN_EXISTING,
        0,
        0);

    if(FileHandle == INVALID_HANDLE_VALUE)
        return;

    UCHAR buf[0x200];
    ULONG ulReturn;
    ReadFile(FileHandle,buf,0x200,&ulReturn,NULL);

    if( buf[0x1fe]!=0x55 || buf[0x1ff]!=0xAA )
        goto Exit;

    PPARTITION_ENTRY lpPtEntry = (PPARTITION_ENTRY)&buf[0x1be];

    for(int i=0; i<4; i++,lpPtEntry++)
    {
        if( lpPtEntry->sys_ind )
        {
            ULONGLONG length = lpPtEntry->nr_sects;
            length = length * 512 / 1024 / 1024;
            printf("StartSector: %d, %I64d MB\n", lpPtEntry->start_sect, length);

            if (lpPtEntry->sys_ind == PARTITION_EXTENDED ||
                lpPtEntry->sys_ind == PARTITION_XINT13_EXTENDED)
            {
            }
            else
            {
                if( lpPtEntry->boot_ind == 0x80 )  // 活动分区
                {
                    LARGE_INTEGER lpNewFilePointer;

                    lpNewFilePointer.QuadPart = lpPtEntry->start_sect*0x200;

                    SetFilePointerEx(FileHandle,lpNewFilePointer,NULL,FILE_BEGIN);

                    ReadFile(FileHandle,buf,0x200,&ulReturn,NULL);

                    PPACKED_BOOT_SECTOR pBootSector = (PPACKED_BOOT_SECTOR)buf;

                    printf( "DBR's: %s, bytes/sector: %d, sector/cluster: %d\n",
                        pBootSector->Oem,
                        pBootSector->PackedBpb.BytesPerSector,
                        pBootSector->PackedBpb.SectorsPerCluster);

                    break;
                }
            }
        }
    }

Exit:
    if (FileHandle)
        CloseHandle(FileHandle);
}


int main()
{
    setlocale(LC_ALL, "");

    AnalyseMBR();
    AnalysePartition();

	printf("\npress any key to exit..");
	getchar();
	return 0;
}

