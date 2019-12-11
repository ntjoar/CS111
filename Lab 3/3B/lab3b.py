import sys,csv,math,numpy

sbEle=None
gdEle=None
inEle=[]
direntEle=[]
idEle=[]
bfreeEle=[]
ifreeEle=[]
blockToInodes={}
dupElement=set()

BLOCK_COUNT = 0
FIRST_BLOCK = 0

class superblock:
    def __init__(self,x):
        self.s_blocks_count     = int(x[1])
        self.s_inodes_count     = int(x[2])
        self.block_size         = int(x[3])
        self.s_inode_size       = int(x[4])
        self.s_blocks_per_group = int(x[5])
        self.s_inodes_per_group = int(x[6])
        self.s_first_ino        = int(x[7])

class inode:
    def __init__(self,x):
        self.inode_num       = int(x[1])
        self.file_type       = x[2]
        self.mode            = int(x[3])
        self.i_uid           = int(x[4])
        self.i_gid           = int(x[5])
        self.i_links_count   = int(x[6])
        self.ctime_str       = x[7]
        self.mtime_str       = x[8]
        self.atime_str       = x[9]
        self.i_size          = int(x[10])
        self.i_blocks        = int(x[11])
        self.dirent_blocks   = [int(block_num) for block_num in x[12:24]]
        self.indirect_blocks = [int(block_num) for block_num in x[24:27]]

class group:
    def __init__(self,x):
        self.s_inodes_per_group = int(x[3])
        self.bg_inode_table     = int(x[-1])

class dirent:
    def __init__(self,x):
        self.inode_num      = int(x[1])
        self.logical_offset = int(x[2])
        self.inode          = int(x[3])
        self.rec_len        = int(x[4])
        self.name_len       = int(x[5])
        self.name           = x[6]

class indirect:
    def __init__(self,x):
        self.inode_num    = int(x[1])
        self.level        = int(x[2])
        self.offset       = int(x[3])
        self.block_number = int(x[4])
        self.element      = int(x[5])


def blockInconsistencies(BLOCK_COUNT, FIRST_BLOCK):
    for x in inEle:
        if x.file_type == 's' and x.i_size <=BLOCK_COUNT:
            continue
        offset = 0
        for bNum in x.dirent_blocks:
            if bNum < 0 or bNum >= BLOCK_COUNT:
                print("INVALID BLOCK %d IN INODE %d AT OFFSET %d" %(bNum, x.inode_num, offset))
            elif bNum > 0 and bNum < FIRST_BLOCK:
                print("RESERVED BLOCK %d IN INODE %d AT OFFSET %d" %(bNum, x.inode_num, offset))
            elif bNum != 0: # find used blocks
                if bNum not in blockToInodes:
                    blockToInodes[bNum] = [[x.inode_num, offset, 0]]
                else:
                    dupElement.add(bNum)
                    blockToInodes[bNum].append([x.inode_num, offset, 0])
            offset += 1
    
        bNum = x.indirect_blocks[0]
        if x.indirect_blocks[0] < 0 or x.indirect_blocks[0] >= BLOCK_COUNT:
            print("INVALID INDIRECT BLOCK %d IN INODE %d AT OFFSET 12" %(bNum, x.inode_num))
        elif bNum > 0 and bNum < FIRST_BLOCK:
            print("RESERVED INDIRECT BLOCK %d IN INODE %d AT OFFSET 12" %(bNum, x.inode_num))
        elif bNum != 0:
            if bNum not in blockToInodes:
                blockToInodes[bNum] = [[x.inode_num, 12, 1]]
            else:
                dupElement.add(bNum)
                blockToInodes[bNum].append([x.inode_num, 12, 1])

        bNum=x.indirect_blocks[1]
        if x.indirect_blocks[1] < 0 or x.indirect_blocks[1] >= BLOCK_COUNT:
            print("INVALID DOUBLE INDIRECT BLOCK %d IN INODE %d AT OFFSET 268" %(bNum, x.inode_num))
        elif bNum > 0 and bNum < FIRST_BLOCK:
            print("RESERVED DOUBLE INDIRECT BLOCK %d IN INODE %d AT OFFSET 268" %(bNum, x.inode_num))
        elif bNum != 0:
            if bNum not in blockToInodes:
                blockToInodes[bNum] = [[x.inode_num, 268, 2]]
            else:
                dupElement.add(bNum)
                blockToInodes[bNum].append([x.inode_num, 268, 2])
        
        bNum=x.indirect_blocks[2]
        if x.indirect_blocks[2] < 0 or x.indirect_blocks[2] >= BLOCK_COUNT:
            print("INVALID TRIPLE INDIRECT BLOCK %d IN INODE %d AT OFFSET 65804" %(bNum, x.inode_num))
        elif bNum > 0 and bNum < FIRST_BLOCK:
            print("RESERVED TRIPLE INDIRECT BLOCK %d IN INODE %d AT OFFSET 65804" %(bNum, x.inode_num))
        elif bNum != 0:
            if bNum not in blockToInodes:
                blockToInodes[bNum] = [[x.inode_num, 65804, 3]]
            else:
                dupElement.add(bNum)
                blockToInodes[bNum].append([x.inode_num, 65804, 3])
    
    for i in idEle:
        bNum = i.element
        lvl = ""
        if i.level == 1:
            lvl = "INDIRECT"
        elif i.level == 2:
            lvl = "DOUBLE INDIRECT"
        elif i.level == 3:
            lvl = "TRIPLE INDIRECT"

        if bNum < 0 or bNum >= BLOCK_COUNT:
            print("INVALID %s BLOCK %d IN INODE %d AT OFFSET %d" %(level, bNum, i.inode_num, i.offset))
        elif bNum > 0 and bNum < FIRST_BLOCK:
            print("RESERVED %s BLOCK %d IN INODE %d AT OFFSET %d" %(level, bNum, i.inode_num, i.offset))
        elif bNum != 0: # find used blocks
            if bNum not in blockToInodes:
                blockToInodes[bNum] = [[i.inode_num, i.offset, i.level]]
            else:
                dupElement.add(bNum)
                blockToInodes[bNum].append([i.inode_num, i.offset, i.level])
        
    for x in range(FIRST_BLOCK, BLOCK_COUNT):
        if x not in bfreeEle and x not in blockToInodes:
            print("UNREFERENCED BLOCK %d" %x)
        if x in bfreeEle and x in blockToInodes:
            print("ALLOCATED BLOCK %d ON FREELIST" %x)

    for n in dupElement:
        for r in blockToInodes[n]:
            id = ""
            if r[-1] == 1:
                id = "INDIRECT"
            elif r[-1] == 2:
                id = "DOUBLE INDIRECT"
            elif r[-1] == 3:
                id = "TRIPLE INDIRECT"
            if id == "":
                print("DUPLICATE BLOCK %d IN INODE %d AT OFFSET %d" %(n, r[0], r[1]))
            else:
                print("DUPLICATE %s BLOCK %d IN INODE %d AT OFFSET %d" %(id, n, r[0], r[1]))

def inodeAndDirectoryInconsistencies(inodev, inodec):
    allocIn=[]
    lns = numpy.zeros(inodev + inodec)
    parentOf = numpy.zeros(inodev + inodec)
    parentOf[2] = 2

    for n in inEle:
        if n.inode_num != 0:
           allocIn.append(n.inode_num)
           if n.inode_num in ifreeEle:
               print("ALLOCATED INODE %d ON FREELIST" %(n.inode_num))
    
    for n in range(inodev, inodec):
        if n not in allocIn and n not in ifreeEle:
            print("UNALLOCATED INODE %d NOT ON FREELIST" %(n))
    
    for i in inEle:
        if i.inode_num not in allocIn and i.inode_num not in ifreeEle:
            emptylist = "empty"
        elif i.file_type == '0' and i.inode_num not in ifreeEle:
            print("UNALLOCATED INODE %d NOT ON FREELIST" %(i.inode_num))
    
    for d in direntEle:
        if d.inode > inodec or d.inode < 1:
            print("DIRECTORY INODE %d NAME %s INVALID INODE %d" %(d.inode_num, d.name, d.inode))
        elif d.inode not in allocIn:
            print("DIRECTORY INODE %d NAME %s UNALLOCATED INODE %d" %(d.inode_num, d.name, d.inode))
        else:
            lns[d.inode] += 1

    for i in inEle:
        if lns[i.inode_num] != i.i_links_count:
            print("INODE %d HAS %d LINKS BUT LINKCOUNT IS %d" %(i.inode_num, lns[i.inode_num], i.i_links_count))

    for d in direntEle:
        if d.name != "'.'" and d.name != "'..'" :
            parentOf[d.inode]=d.inode_num
        
    for d in direntEle:
        if d.name == "'.'" and d.inode != d.inode_num:
            print("DIRECTORY INODE %d NAME '.' LINK TO INODE %d SHOULD BE %d"  %(d.inode_num, d.inode,d.inode_num))
        if d.name == "'..'" and parentOf[d.inode_num] != d.inode:
            print("DIRECTORY INODE %d NAME '..' LINK TO INODE %d SHOULD BE %d" %(d.inode_num, d.inode, parentOf[d.inode_num]))


def printInconsistencies(BLOCK_COUNT, FIRST_BLOCK, inodev, inodec):
    blockInconsistencies(BLOCK_COUNT, FIRST_BLOCK)
    inodeAndDirectoryInconsistencies(inodev, inodec)

def main ():
    if len(sys.argv) != 2:
        sys.stderr.write("Error: wrong number of arguments.\N")
        sys.exit(1)
    
    try:
        inFile=open(sys.argv[1], 'r')
    except IOError:
        sys.stderr.write("Error: cannot open the input file.\n")
        sys.exit(1)

    fr = csv.reader(inFile)
    
    for x in fr:
        if x[0] == "SUPERBLOCK":
            sbEle=superblock(x)
        elif x[0] == "GROUP":
            gdEle=group(x)
        elif x[0] == "BFREE":
            bfreeEle.append(int(x[1]))
        elif x[0] == "IFREE":
            ifreeEle.append(int(x[1]))
        elif x[0] == "INODE":
            inEle.append(inode(x))
        elif x[0] == "DIRENT":
            direntEle.append(dirent(x))
        elif x[0] == "INDIRECT":
            idEle.append(indirect(x))
        else:
            sys.stderr.write("Error: CSV File contains invalid input.")
            sys.exit(1)
        
    BLOCK_COUNT = sbEle.s_blocks_count
    FIRST_BLOCK = int(math.ceil(gdEle.s_inodes_per_group * sbEle.s_inode_size / sbEle.block_size) + gdEle.bg_inode_table)
    
    printInconsistencies(BLOCK_COUNT, FIRST_BLOCK, sbEle.s_first_ino, sbEle.s_inodes_count)

if __name__ == '__main__':
    main()