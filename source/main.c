#include <nds.h>
#include <fat.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

#define VERSION "2.1.0"
#define RWCHUNK	(2048*512) //2048 sectors (1 MB)

//Function index________________________
int main();
int checkNCSD();
void dump3dsNand(int mode);
void restore3dsNand(int mode);
void xorbuff(u8 *in1, u8 *in2, u8 *out);
void installB9S();
u32 handleUI();
u32 waitNandWriteDecision();
void error(int code);
//______________________________________

const char *green="\x1b[32;1m";
const char *yellow="\x1b[33;1m";
const char *blue="\x1b[34;1m";
const char *white="\x1b[37;1m";

int menu_index=0;
u32 sysid=0;  //NCSD magic
u32 ninfo=0;  //nand size in sectors
u32 sizeMB=0; //nand/firm size in MB
u32 System=0; //will be one of the below 2 variables
u32 N3DS=2;
u32 O3DS=1;
char workdir[]= "boot9strap";
char nand_type[80]={0};   //sd filename buffer for nand/firm dump/restore.
u8 *workbuffer; //raw dump and restore in first two options
u8 *fbuff; //sd firm files
u8 *xbuff; //xorpad
u8 *nbuff; //nand
PrintConsole topScreen;
PrintConsole bottomScreen;

const char *boot9strap="boot9strap.firm";
const char *fnameOLD="2.54-0_11.4_OLD.firm";
const char *fnameNEW="2.54-0_11.4_NEW.firm";
const char *fnum="11.4.0";

int main() {
	videoSetMode(MODE_0_2D);    //first 7 lines are all dual screen printing init taken from libnds's nds-examples
	videoSetModeSub(MODE_0_2D);

	vramSetBankA(VRAM_A_MAIN_BG);
	vramSetBankC(VRAM_C_SUB_BG);

	consoleInit(&topScreen, 3,BgType_Text4bpp, BgSize_T_256x256, 31, 0, true, true);
	consoleInit(&bottomScreen, 3,BgType_Text4bpp, BgSize_T_256x256, 31, 0, false, true);
    consoleSelect(&topScreen);

	if (!fatInitDefault()) error(0);

	workbuffer = (u8*)memalign(RWCHUNK,32);  //setup and allocate our buffers. 4 x 1MB. the dsi and 3ds can handle this given 16MB ram. should crash ds.
	fbuff = (u8*)memalign(RWCHUNK,32);
	nbuff = (u8*)memalign(RWCHUNK,32);
	xbuff = (u8*)memalign(RWCHUNK,32);

	mkdir(workdir, 0777);   //b9sTool folder creation
	chdir(workdir);

	checkNCSD();     //read ncsd header for needed info

	while(handleUI());  //game loop

	free(workbuffer); free(fbuff); free(nbuff); free(xbuff);
	return 0;
}

int checkNCSD() {
	nand_ReadSectors(0 , 1 , workbuffer);   //get NCSD (nand) header of the 3ds
	memcpy(&sysid, workbuffer + 0x100, 4);  //NCSD magic
	memcpy(&ninfo, workbuffer + 0x104, 4);  //nand size in sectors (943 or 1240 MB). used to determine old/new 3ds.
	if     (ninfo==0x00200000){ System=O3DS;}    //old3ds
	else if(ninfo==0x00280000){ System=N3DS;}    //new3ds
	if(!System || sysid != 0x4453434E)error(2);  //this error triggers if NCSD magic not present or nand size unexpected value.

	if(System==O3DS){  //checking for firm files, 10.4 and 11.0 - 11.2 as of this writing.
		if(access(boot9strap,F_OK) == -1)return -1;  //these errors allowed to pass so users can still dump/restore nand.
		if(access(fnameOLD,F_OK) == -1)return -1;
	}
	else if(System==N3DS){
		if(access(boot9strap,F_OK) == -1)return -1;
		if(access(fnameNEW,F_OK) == -1)return -1;
	}
	else{
		return -1;
	}

  return 0;
}

void dump3dsNand(int mode) {
	consoleClear();
	u32 foffset;
	if(mode){                                   //full nand dump/restore (mode=1).
		foffset=0x0;
		if(System==O3DS){
			sizeMB=943;
			strcpy(nand_type,"NAND_OLD3DS.BIN");
		}
		else{  //System==N3DS
			sizeMB=1240;
			strcpy(nand_type,"NAND_NEW3DS.BIN");
		}
	}
	else{                                      //firm0 and firm1 dump/restore (mode=0)
		foffset=0x0B130000/0x200;
		if(System==O3DS){
			sizeMB=8;
			strcpy(nand_type,"F0F1_OLD3DS.BIN");
		}
		else {
			sizeMB=8;
			strcpy(nand_type,"F0F1_NEW3DS.BIN");
		}
	}
	iprintf("opening b9sTool/%s\n",nand_type);
	iprintf("hold B to cancel\n");

	FILE *f = fopen(nand_type, "wb");
	if(!f)error(1);

	u32 rwTotal=sizeMB*1024*1024;

	for(int i=0;i<rwTotal;i+=RWCHUNK){           //read from nand, dump to sd
		if(nand_ReadSectors(i / 0x200 + foffset, RWCHUNK / 0x200, workbuffer) == false){
			iprintf("nand read error\noffset: %08X\naborting...", (int)i);
			unlink(nand_type);
			break;
		}
		if(fwrite(workbuffer, 1, RWCHUNK, f) < RWCHUNK){
			iprintf("sdmc write error\noffset: %08X\naborting...", (int)i);
			unlink(nand_type);
			break;
		}
		iprintf("progress %d/%dMBs   \r",i / 0x100000 + 1, (int)rwTotal / 0x100000);
		scanKeys();
		int keys = keysHeld();
		if(keys & KEY_B){
			iprintf("\ncanceling...");
			unlink(nand_type);
			break;
		}
	}

	fclose(f);
	iprintf("\ndone.\r");
}

void restore3dsNand(int mode) {
	consoleClear();
	u32 foffset;
	if(mode){
		foffset=0x0;
		if(System==O3DS){
			sizeMB=943;
			strcpy(nand_type,"NAND_OLD3DS.BIN");
		}
		else{
			sizeMB=1240;
			strcpy(nand_type,"NAND_NEW3DS.BIN");
		}
	}
	else{
		foffset=0x0B130000/0x200;
		if(System==O3DS){
			sizeMB=8;
			strcpy(nand_type,"F0F1_OLD3DS.BIN");
		}
		else {
			sizeMB=8;
			strcpy(nand_type,"F0F1_NEW3DS.BIN");
		}
	}

	int rerror=0;
	int werror=0;

	if(waitNandWriteDecision())return;
	consoleClear();

	iprintf("opening b9sTool/%s\n",nand_type);
	iprintf("do NOT poweroff!!\n");

	FILE *f = fopen(nand_type, "rb");
	if(!f)error(1);

	u32 rwTotal=sizeMB*1024*1024;

	for(int i=0;i<rwTotal;i+=RWCHUNK){
		if(fread(workbuffer, 1, RWCHUNK, f) < RWCHUNK){
			rerror++;
			continue;  //better not to write anything if it's bad
		}
		if(nand_WriteSectors(i / 0x200 + foffset, RWCHUNK / 0x200, workbuffer) == false){
			werror++;
		}
		iprintf("progress %d/%dMBs   \r",i / 0x100000 + 1, (int)rwTotal / 0x100000);
	}

	fclose(f);
	iprintf("\nreadErrors=%d\nwriteErrors=%d\n",rerror, werror);
	iprintf("done.\n");
	if(rerror || werror) iprintf("if there's any errors, it's\nbest to run this again now.");
}

void xorbuff(u8 *in1, u8 *in2, u8 *out){

	for(int i=0; i < RWCHUNK; i++){
		out[i] = in1[i] ^ in2[i];
	}

}

void installB9S() {
	consoleClear();
	sizeMB=4;

	iprintf("%sWILL BRICK%s if a9lh is already\n", yellow, white);
	iprintf("installed!\n\n");

	if(waitNandWriteDecision())return;
	consoleClear();

	FILE *f104;
	FILE *fxxx;

	if     (System==O3DS){
		iprintf("opening %s/%s\nand          */%s\n", workdir, boot9strap,fnameOLD);
		f104 = fopen(boot9strap,"rb");
		fxxx = fopen(fnameOLD,"rb");
	}
	else {
		iprintf("opening %s/%s\nand          */%s\n", workdir, boot9strap,fnameNEW);
		f104 = fopen(boot9strap,"rb");
		fxxx = fopen(fnameNEW,"rb");
	}

	if (!fxxx || !f104) error(3);

	u32 foffset=0x0B130000/0x200; //firm0 nand offset
	u32 rwTotal=sizeMB*1024*1024;

	for (int i=0; i < rwTotal; i+=RWCHUNK) {

		fread(fbuff, 1, RWCHUNK, fxxx);                                 //get dec firm 11.x on sd
		nand_ReadSectors(i / 0x200 + foffset, RWCHUNK / 0x200, nbuff);  //get enc firm 11.x on nand
		xorbuff(fbuff,nbuff,xbuff);                                     //xor the above two buffs to create xorpad buff
		fread(fbuff, 1, RWCHUNK, f104);                                 //get dec firm 10.4 on sd
		xorbuff(fbuff,xbuff,nbuff);                                     //xor dec sd firm 10.4 and xorpad to create final encrypted image to write to nand
		nand_WriteSectors(i / 0x200 + foffset, RWCHUNK / 0x200, nbuff); //write to nand

		iprintf("%d/%dMBs DON'T poweroff!\r", i / 0x100000 + 1, (int)rwTotal / 0x100000);
	}

	fclose(f104);
	fclose(fxxx);

	iprintf("\ndone.");
}

u32 handleUI(){
	consoleSelect(&topScreen);
	consoleClear();

	const int menu_size=6;
	const char menu[][32]={
		{"Exit\n"},
		{"Install boot9strap\n"},
		{"Dump    NAND"},
		{"Restore NAND\n"},
		{"Dump    F0F1"},
		{"Restore F0F1"},
	};

	iprintf("b9sTool %s\n", VERSION);
	if    (System==O3DS)iprintf("%sOLD 3DS%s\n", yellow, white);
	else                iprintf("%sNEW 3DS%s\n", yellow, white);
	iprintf("%s%s%s\n\n", blue, fnum, white);

	for(int i=0;i<menu_size;i++){
		iprintf("%s%s\n", i==menu_index ? " > " : "   ", menu[i]);
	}

	swiWaitForVBlank();
	scanKeys();
	int keypressed=keysDown();

	if     (keypressed & KEY_DOWN)menu_index++;
	else if(keypressed & KEY_UP)  menu_index--;
	else if(keypressed & KEY_A){
		consoleSelect(&bottomScreen);
		if     (menu_index==0)return 0;          //break game loop and exit app
		else if(menu_index==1)installB9S();
		else if(menu_index==2)dump3dsNand(1);    // dump/restore full nand
		else if(menu_index==3)restore3dsNand(1);
		else if(menu_index==4)dump3dsNand(0);    // dump/restore firm0 and firm1
		else if(menu_index==5)restore3dsNand(0);
	}
	if(menu_index >= menu_size)menu_index=0;     //menu index wrap around check
	else if(menu_index < 0)    menu_index=menu_size-1;

	return 1;  //continue game loop
}

u32 waitNandWriteDecision(){
	iprintf("NAND writing is DANGEROUS!\n");
	iprintf("START+SELECT = %sGO!%s\n", green, white);
	iprintf("B to decline.\n");
	while(1){
		scanKeys();
		int keys = keysHeld();
		if((keys & KEY_START) && (keys & KEY_SELECT))break;
		if(keys & KEY_B){
			consoleClear();
			return 1;
		}
		swiWaitForVBlank();
	}
	return 0;
}

void error(int code){
	switch(code){
		case 0: iprintf("Fat could not be initialized!\n"); break;
		case 1: iprintf("Could not open file handle!\n"); break;
		case 2: iprintf("Not a 3ds (or nand read error)!\n"); break;
		case 3: iprintf("Could not open sdmc files!\n"); break;
		default: break;
	}

	iprintf("Press home to exit\n");
	free(workbuffer); free(fbuff); free(nbuff); free(xbuff);
	while(1)swiWaitForVBlank();
}
