//
//  main.c
//  DisARM
//
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "elf.h"

int Disassemble(unsigned int *armI, int count, unsigned int startAddress);
void DecodeInstruction(unsigned int instr, unsigned int startAddress);
void DecodeDataProcessing(unsigned int instr, int ccode);
int SignExtend(unsigned int x, int bits);
int Rotate(unsigned int rotatee, int amount);

int main(int argc, const char * argv[])
{
    int startNextAdd = 0;
    FILE *fp;
    ELFHEADER elfhead;
    ELFPROGHDR *prgHdr;
    int i;
    unsigned int *armInstructions = NULL;
    
    if(argc < 2)
    {
        fprintf(stderr, "Usage: DisARM <filename>\n");
        return 1;
    }
    
    /* Open ELF file for binary reading */
    fp = fopen(argv[1], "rb");
    
    /* Read in the header */
    fread(&elfhead, 1, sizeof(ELFHEADER), fp);
    if(!(elfhead.magic[0] == 0177 && elfhead.magic[1] == 'E' && elfhead.magic[2] == 'L' && elfhead.magic[3] == 'F'))
    {
        fprintf(stderr, "%s is not an ELF file\n", argv[1]);
        return 2;
    }
    
    printf("File-type: %d\n",elfhead.filetype);
    printf("Arch-type: %d\n",elfhead.archtype);
    printf("Entry: %x\n", elfhead.entry);
    printf("Prog-Header: %x\n", elfhead.phdrpos);
    printf("Prog-Header-count: %d\n", elfhead.phdrcnt);
    printf("Section-Header: %x\n", elfhead.shdrpos);
    
// You might have noticed from the ELF header definition above how there were fields 
// e_phoff, e_phnum and e_phentsize; these are simply the offset in the file where the program headers start, 
// how many program headers there are and how big each program header is. With these three bits of information 
// you can easily find and read the program headers.


    /* Find and read program headers */
    fseek(fp, elfhead.phdrpos, SEEK_SET);
    prgHdr = (ELFPROGHDR*)malloc(sizeof(ELFPROGHDR) * elfhead.phdrcnt);
    if(!prgHdr)
    {
        fprintf(fp, "Out of Memory\n");
        fclose(fp);
        return 3;
    }
    
    fread(prgHdr, elfhead.phdrcnt, sizeof(ELFPROGHDR), fp);
    for (i = 0; i < elfhead.phdrcnt; i++)
    {

        printf("Segment-Offset: %x\n", prgHdr[i].offset);
        printf("File-size: %d\n", prgHdr[i].filesize);
        printf("Align: %d\n", prgHdr[i].align);
        
        /* allocate memory and read in ARM instructions */
        armInstructions = (unsigned int *)malloc(prgHdr->filesize + 3 & ~3);
        if(armInstructions == NULL)
        {
            fclose(fp);
            free(prgHdr);
            fprintf(stderr, "Out of Memory\n");
            return 3;
        }
        fseek(fp, prgHdr->offset, SEEK_SET + i * sizeof(armInstructions));
        fread(armInstructions, 1, prgHdr->filesize, fp);
        
        /* Disassemble */
        printf("Instructions\n============\n\n");
        
        startNextAdd = Disassemble(armInstructions, (prgHdr->filesize + 3 & ~3) /4, ((prgHdr->virtaddr) + startNextAdd * 4));
        printf("\n");
        
        free(armInstructions);
    }

    free(prgHdr);
    fclose(fp);

    return 0;
}

int Disassemble(unsigned int *armI, int count, unsigned int startAddress)
{
    int i;
    
    for(i = 0; i < count; i++)
    {
        printf("%08x: %08x ", startAddress + i*4, armI[i]);
        
        DecodeInstruction(armI[i], startAddress + i*4);
        
        printf("\n");
    }
    return i;
    
}



char* hexToBin(int v, int HexSize) 
{   int i;
    for(i = (sizeof(v)*HexSize)-1; i >= 0; i--)
    putchar('0' + ((v >> i) & 1));
    printf(" ");
} 
 
void printConCode(int contcode)
{
    char conditionopcode[16][3] = {"EQ", "NE", "CS", "CC", "MI", "PL", "VS", "VC", "HI", "LS", "GE", "LT", "GT", "LE", "  "}; 
    printf("%s",conditionopcode[contcode]);
}

void printDPcode(int DPIopcode)
{
    char DPInstruction[16][4] = {"AND","EOR", "SUB", "RSB", "ADD", "ADC", "SBC", "RSC", "TST", "TEQ", "CMP", "CMN", "ORR", "MOV", "BIC", "MVN"};
    printf("%s",DPInstruction[DPIopcode]);
}

void DecodeInstruction(unsigned int instr, unsigned int currAddress)
{
    //hexToBin(instr, 8); // prints out the full binary code
   
    int contcode = (instr & 0xF0000000) >> 28; // check cont-code
    char link[] = ""; // for looking pretty
    int Rs, Rm;
    int shiftType = (instr & 0x00000060); // sh 
    char shiftTypeDP[4][4] = {"LSR","LSL", "ASR", "ROR"};
    char Op[256];
    char Op2[256];

    char Rn[256], Rd[256];
    sprintf(Rd, "R%d", ((instr & 0x0000F000) >> 12));
    sprintf(Rn, "R%d", ((instr & 0x000F0000) >> 16));
    if(strcmp(Rn, "R15") == 0)     // can put in function
    {
        sprintf(Rn, "PC");
    }
    if(strcmp(Rn, "R14") == 0)
    {
        sprintf(Rn, "LR");
    }
    if (strcmp(Rd, "R15") == 0)
    {
        sprintf(Rd, "PC");
    }
    if(strcmp(Rd, "R14") == 0)
    {
        sprintf(Rd, "LR");
    }

    if (!((instr & 0x0FC00000) >> 22) && ((instr & 0x000000F0) >> 4) == 9) // not true because 000000 == 0 which is false in C
    {
        switch(((instr & 0x00100000) >> 21))
        {
            case 0: //MUL Rd, Rm, Rs
            // no Rn
             printf("MUL   %s, R%d, R%d", Rn, (instr & 0x0000000F), ((instr & 0x00000F00) >> 8)); 
            break; 

            case 1: //MLA Rd, Rm, Rs, Rn
            printf("MLA   %s, R%d, R%d, %s", Rn, (instr & 0x0000000F), ((instr & 0x00000F00) >> 8), Rd); 
            // note these(Rn and Rd) are the other way around becauase of previous instr
           break;
        }
    }
    else
    {
        switch(((instr & 0x0E000000) >> 25))
        {
            case 7: // SWI
                printf("SWI");
                printConCode(contcode);
                printf(" %d",(instr & 0x00FFFFFF));
                break;

            case 5: // Branch
                if (((instr & 0x01000000) >> 24) == 1)
                {
                    strcpy(link,"L");   
                }
                int wordOffset = SignExtend(((instr & 0x00FFFFFF) << 2), 26); // 24 bits, shift 2 to left creating 26 bits
                wordOffset = (currAddress + wordOffset + 8);

                printf("B%s", link);
                printConCode(contcode);
                if (strcmp(link, "L") != 0)
                    printf(" ");
                printf("  &%X", wordOffset);
                break;

            case 0: // Data processing instr.
            case 1:
                switch((instr & 0x02000000) >> 25) // if bit 25 is a 0 op2 is a register - else int
                {
                    case 1: // 3rd value is a immediate value (#10 not R10)
                        /* Op = # && Op */
                        sprintf(Op, "#%X", (Rotate((instr & 0x000000FF), (((instr & 0x00000F00) >> 8) * 2))));
                    break;

                    case 0: // 3rd value  is a register
                    Rm = (instr & 0x0000000F); 
                       switch(((instr & 0x00000010) >> 4)) // decide between shift register or amount
                        {
                            case 0: // shift amount #10 
                                sprintf(Op2, "%s, #%d", shiftTypeDP[shiftType], ((instr & 0x00000F80) >> 7));                            
                                break;
                            case 1: // shift register Rx   
                                Rs = (instr & 0x00000F00) >> 8;
                                sprintf(Op2, "%s, R%d", shiftTypeDP[shiftType], Rs);
                                
                            break;
                        }
                        /* Op = R && Op */
                    sprintf(Op, "R%d", Rm);
                    break; // break from the case 0
                }
                int DPIopcode = (instr & 0x01E00000) >> 21; 
                printDPcode(DPIopcode);
                printConCode(contcode);
                if(((instr & 0x00100000) >> 20) == 1 && DPIopcode != 10)//!= CMP
                {
                    printf("S");
                }

                printf(" ");

                switch(DPIopcode) 
                {
                    case 2: // --- SUB
                    case 3: // --- RSB
                    case 4: // --- ADD Rd, Rn, Op
                        printf("%s, %s, %s", Rd, Rn, Op); //
                        break;
                    case 0: // --- AND Rd, Rn, Op
                    case 1: // --- EOR
                    case 5: // --- ADC Rd, Rn, Op
                    case 6: // --- SBC 
                    case 7: // --- RSC
                    case 12: // --- ORR  
                    case 14: // --- BIC
                        printf("%s, %s, %s", Rd, Rn, Op);//
                    break;
                    case 8: // --- TST Rn, Op -- AS ADD/SUB.. but result is not written
                    case 9: // --- TEQ
                    case 10: // --- CMP
                    case 11: // --- CMN
                        printf("%s, %s", Rn, Op);//
                        break;
                    case 15: // --- MVN Rd, Op
                    case 13: // --- MOV 
                        printf("%s, %s", Rd, Op);
                        break;
                }
                break; // from the case 'D':
            case 2: // Load / Store
            case 3:
            {
                int offsetLS = (instr & 0x02000000) >> 25;
                int p = (instr & 0x01000000) >> 24; //
                int u = (instr & 0x00800000) >> 23; //
                int b = (instr & 0x00400000) >> 22; //
                int w = (instr & 0x00200000) >> 21;
                int l = (instr & 0x00100000) >> 20; //
                switch(l)
                {

                    case 0:
                        printf("STR");
                    break;

                    case 1:
                        printf("LDR");

                    break;


                }
                if (b == 1)
                {
                    printf("B");
                }
                printConCode(contcode);
                printf(" %s, [%s", Rd, Rn);
                if (p == 0)
                {
                    printf("]");
                }
                switch(offsetLS)
                {
                    case 0: // 12 bit immediate
                    if ((instr & 0x00000FFF) != 0)
                    {
                        printf(" #");
                        if (u == 0)
                        {
                            printf("-");
                        }

                        printf("%d", (instr & 0x00000FFF));
                    }
                    break;

                    case 1:
                        Rm = (instr & 0x0000000F); 
                        printf(", R%d", Rm);
                        if ((instr & 0x00000F00) != 0)
                        {   
                            printf(", %s", shiftTypeDP[((instr & 0x00000F00) >> 8)]);
                            printf(" #%d", ((instr & 0x00000F00) >> 8));
                            
                        }
                        break;
                }
                if (p == 1)
                {
                    printf("]");
                    if (w == 1)
                    {
                        printf("!");
                    }
                }

                break;
            }
            default:
                /* error */
                printf("DEF");
                break;
        }
    }
}

int SignExtend(unsigned int x, int bits)
{
    int r;
    int m = 1U << (bits - 1);
    
    x = x & ((1U << bits) - 1);
    r = (x ^ m) - m;
    return r;
}
               
int Rotate(unsigned int rotatee, int amount)
{
    unsigned int mask, lo, hi;

    mask = (1 << amount) - 1;
    lo = rotatee & mask;
    hi = rotatee >> amount;

    rotatee = (lo << (32 - amount)) | hi;
    
    return rotatee;
}