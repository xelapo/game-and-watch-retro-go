#!/usr/bin/env python3
import os
import sys

SecSizes=[128,256,512,1024,4096,0]
FDIDiskLabel = ""

def compress_lzma(data, level=None):
    import lzma
    if level == None or level != 'lzma':
        return data

    compressed_data = lzma.compress(
        data,
        format=lzma.FORMAT_ALONE,
        filters=[
            {
                "id": lzma.FILTER_LZMA1,
                "preset": 6,
                "dict_size": 16 * 1024,
            }
        ],
    )

    compressed_data = compressed_data[13:]

    return compressed_data

def createFDI(Sides,Tracks,Sectors,SecSize,diskBytesArray,compress=None):
    # Find sector size code
    L = 0
    print("disk size =",len(diskBytesArray))
    while SecSizes[L] and SecSizes[L]!=SecSize:
        if SecSizes[L]==0: return bytearray(0)
        L = L + 1

    # Allocate memory */
    diskDataLength = int(Sides*Tracks*Sectors*SecSize+len(FDIDiskLabel))
    fdiDataLength = int(Sides*Tracks*(Sectors+1)*7+14) # 14 = header size
    fdiDiskArray = bytearray(fdiDataLength+diskDataLength)

    # FDI magic number
    fdiDiskArray[0] = ord('F')
    fdiDiskArray[1] = ord('D')
    fdiDiskArray[2] = ord('I')

    # Disk description
    fdiDiskArray[fdiDataLength:fdiDataLength+len(FDIDiskLabel)] = FDIDiskLabel.encode()
    #Write protection (1=ON)
    fdiDiskArray[3]  = 1
    #Set compression byte (not part of the FDI specification)
    if compress == 'lzma':
        fdiDiskArray[3]  +=2
    fdiDiskArray[4]  = Tracks&0xFF
    fdiDiskArray[5]  = Tracks>>8
    fdiDiskArray[6]  = Sides&0xFF
    fdiDiskArray[7]  = Sides>>8
    # Disk description offset
    fdiDiskArray[8]  = fdiDataLength&0xFF
    fdiDiskArray[9]  = fdiDataLength>>8
    # Sector data offset
    fdiDataLength += len(FDIDiskLabel)
    fdiDiskArray[10] = fdiDataLength&0xFF
    fdiDiskArray[11] = fdiDataLength>>8
    # Track directory offset
    fdiDiskArray[12] = 0
    fdiDiskArray[13] = 0

    offset = 14
    track = 0
    trackAddress = 0
    currentAddress = 0
    fdiDataWriteOffset = fdiDataLength
    diskDataReadOffset = 0
    while track<Sides*Tracks :
        # Create track entry
        fdiDiskArray[offset+0] = trackAddress&0xFF
        fdiDiskArray[offset+1] = (trackAddress>>8)&0xFF
        fdiDiskArray[offset+2] = (trackAddress>>16)&0xFF
        fdiDiskArray[offset+3] = (trackAddress>>24)&0xFF
        # Reserved bytes
        fdiDiskArray[offset+4] = 0
        fdiDiskArray[offset+5] = 0
        fdiDiskArray[offset+6] = Sectors
        # For all sectors on a track...
        sectorId = 0
        sectorAddress = 0
        offset+=7
        trackData = compress_lzma(diskBytesArray[diskDataReadOffset:diskDataReadOffset+SecSize*Sectors],compress)
        # write track data at correct address
        fdiDiskArray[fdiDataWriteOffset:fdiDataWriteOffset+len(trackData)] = trackData
        while sectorId<Sectors:
            # Create sector entry
            fdiDiskArray[offset+0] = int(track/Sides)
            fdiDiskArray[offset+1] = track%Sides
            fdiDiskArray[offset+2] = sectorId+1
            fdiDiskArray[offset+3] = L
            # CRC marks and "deleted" bit (D00CCCCC)
            fdiDiskArray[offset+4] = (1<<L)
            fdiDiskArray[offset+5] = sectorAddress&0xFF
            fdiDiskArray[offset+6] = sectorAddress>>8

            sectorId+=1
            offset+=7
            sectorAddress+=SecSize
            diskDataReadOffset+=SecSize

        fdiDataWriteOffset+=len(trackData)
        trackAddress+=len(trackData)
        track+=1

    return fdiDiskArray[0:fdiDataWriteOffset]

def analyzeRawDsk(dskFile,fileName):
    dskFile.seek(0, os.SEEK_END)
    size = dskFile.tell()
    dskFile.seek(0, os.SEEK_SET)

    if size == 2*80*9*512:
        diskArray = createFDI(2,80,9,512,dskFile.read(),compress)
    else :
        diskArray = bytearray(0)

    return diskArray

n = len(sys.argv)

if n < 2: print("Usage :\ndsk2fdi.py file.dsk [compress]\n"); sys.exit(0)
if n == 2:
    compress=None
else:
    compress=sys.argv[2]

# Open file and find its size
print("Opening "+sys.argv[1])
dskFile = open(sys.argv[1], 'rb')
fdiArray = analyzeRawDsk(dskFile,sys.argv[1])
if len(fdiArray) > 0 :
    outFile = sys.argv[1]+".fdi"
    print("File converted, saving "+outFile)
    newfile=open(outFile,'wb')
    newfile.write(fdiArray)
    newfile.close()
    dskFile.close()
    print("Done")
    sys.exit(0)

print("Failed")
#TODO : check if file is not already a FDI file

dskFile.close()
sys.exit(-1)
