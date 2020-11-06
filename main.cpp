
#include "ParseNtfs.h"

void AnalyseMBR()
{
    HANDLE hFileDrive0 = CreateFileW(
        L"\\\\.\\PhysicalDrive0",
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        0,
        OPEN_EXISTING,
        0,
        0);

    if(hFileDrive0 == INVALID_HANDLE_VALUE)
        return;

    UCHAR buf[0x200];
    ULONG LenReaded;
    ReadFile(hFileDrive0,buf,sizeof(buf),&LenReaded,NULL);

    if( buf[0x1fe]!=0x55 || buf[0x1ff]!=0xAA )
        goto Exit;

    PPARTITION_ENTRY pPartEntry = (PPARTITION_ENTRY)&buf[0x1be];

    for(int i=0; i<4; i++,pPartEntry++)
    {
        if( pPartEntry->sys_ind )
        {
            ULONGLONG length = pPartEntry->nr_sects;
            length = length * 512 / 1024 / 1024;
            printf("StartSector: %d, %I64d MB\n", pPartEntry->start_sect, length);

            if (pPartEntry->sys_ind == PARTITION_EXTENDED ||
                pPartEntry->sys_ind == PARTITION_XINT13_EXTENDED)
            {
            }
            else
            {
                if ( pPartEntry->boot_ind == 0x80 )  // 活动分区
                {
                    LARGE_INTEGER liNewFilePointer;

                    liNewFilePointer.QuadPart = pPartEntry->start_sect*0x200;

                    SetFilePointerEx(hFileDrive0,liNewFilePointer,NULL,FILE_BEGIN);

                    ReadFile(hFileDrive0,buf,sizeof(buf),&LenReaded,NULL);

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
    if (hFileDrive0)
        CloseHandle(hFileDrive0);
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

