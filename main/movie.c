/*
THE COMPUTER CODE CONTAINED HEREIN IS THE SOLE PROPERTY OF PARALLAX
SOFTWARE CORPORATION ("PARALLAX").  PARALLAX, IN DISTRIBUTING THE CODE TO
END-USERS, AND SUBJECT TO ALL OF THE TERMS AND CONDITIONS HEREIN, GRANTS A
ROYALTY-FREE, PERPETUAL LICENSE TO SUCH END-USERS FOR USE BY SUCH END-USERS
IN USING, DISPLAYING,  AND CREATING DERIVATIVE WORKS THEREOF, SO LONG AS
SUCH USE, DISPLAY OR CREATION IS FOR NON-COMMERCIAL, ROYALTY OR REVENUE
FREE PURPOSES.  IN NO EVENT SHALL THE END-USER USE THE COMPUTER CODE
CONTAINED HEREIN FOR REVENUE-BEARING PURPOSES.  THE END-USER UNDERSTANDS
AND AGREES TO THE TERMS HEREIN AND ACCEPTS THE SAME BY USE OF THIS FILE.  
COPYRIGHT 1993-1999 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/


#pragma off (unreferenced)
static char rcsid[] = "$Id: movie.c 1.76 1996/08/28 17:34:03 jed Exp $";
#pragma on (unreferenced)

// Without this, you'll have a bad time on Android with SIGSEGV when reading files
#define __USE_BSD

#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#ifdef ANDROID_NDK
#include <linux/limits.h>
#else
#include <sys/syslimits.h>
#endif

#include "pa_enabl.h"                   //$$POLY_ACC
#include "inferno.h"
#include "text.h"
#include "args.h"
#include "mem.h"
#include "byteswap.h"
#include "cfile.h"
#include "gamefont.h"
#include "gr.h"
#include "palette.h"
#include "config.h"
#include "mono.h"
#include "error.h"
#include "digi.h"
#include "songs.h"
#include "timer.h"
#include "joy.h"
#include "key.h"
#include "movie.h"
#include "screens.h"
#include "newmenu.h"
#include "menu.h"
#include "libmve.h"
#include "strutil.h"
#include "ViewControllerC.h"
#include "mouse.h"

#if defined(POLY_ACC)
#include "poly_acc.h"
#endif

extern char *Args[100];                                 // Arguments from args.c for
char *RoboBuffer[50];
char RobBufCount=0,PlayingBuf=0,RobBufLimit=0;

unsigned RobSX=75,RobSY=50,RobDX=100,RobDY=100;
FILE *RoboFile = NULL;
int RoboFilePos = 0, MVEPaletteCalls = 0;

//      Function Prototypes
int RunMovie(char *filename, int highres_flag, int allow_abort,int dx,int dy);
extern void do_briefing_screens (char *,int);

// Subtitle data
typedef struct {
	short first_frame,last_frame;
	char *msg;
} subtitle;


// #define BUFFER_MOVIE 

#define MAX_SUBTITLES 500
subtitle Subtitles[MAX_SUBTITLES];
int Num_subtitles;

// ----------------------------------------------------------------------
void* MPlayAlloc(unsigned size)
{
    return malloc(size);
}

void MPlayFree(void *p)
{
    free(p);
}

int HiResRoboMovie=0;

//-----------------------------------------------------------------------

unsigned FileRead(void *handle, void *buf, unsigned count)
{
    unsigned numread;
    numread = fread(buf, 1, count, handle);
    return (numread == count);
}

#define VID_PLAY 0
#define VID_PAUSE 1

int Vid_State;

extern int Digi_initialized;
extern int digi_timer_rate;

int MovieHires = 1;		//default for now is hires

#define MOVIE_VOLUME_SCALE  (32767)		//32767 is MAX

//filename will actually get modified to be either low-res or high-res
//returns status.  see values in movie.h
int PlayMovie(const char *filename, int must_have)
{
	char name[FILENAME_LEN],*p;
	int c,ret;
	int save_sample_rate;

	#if defined(POLY_ACC)
		PA_DFX (pa_set_write_mode (1));
		PA_DFX (pa_set_frontbuffer_current());
	#endif

	if (!Digi_initialized)
		return MOVIE_NOT_PLAYED;

	#ifndef RELEASE
	if (FindArg("-nomovies"))
		return MOVIE_NOT_PLAYED;
	#endif

	strcpy(name,filename);

	if ((p=strchr(name,'.')) == NULL)		//add extension, if missing
		strcat(name,".mve");

	//check for escape already pressed & abort if so
	while ((c=key_inkey()) != -1)
		if (c == KEY_ESC)
			return MOVIE_ABORTED;

	// Stop all digital sounds currently playing.
	digi_stop_all();

	// Stop all songs
	songs_stop_all();

	save_sample_rate = digi_sample_rate;
	digi_sample_rate = SAMPLE_RATE_22K;		//always 22K for movies
	digi_reset(); digi_reset();

	// Start sound 
	/*if (hSOSDigiDriver < 0xffff) {
		MVE_SOS_sndInit(hSOSDigiDriver);
		MVE_sndVolume((Config_digi_volume*MOVIE_VOLUME_SCALE)/8);
	}
	else
		MVE_SOS_sndInit(-1);

	MVE_rmFastMode (MVE_RM_NORMAL);*/

	ret = RunMovie(name,MovieHires,must_have,-1,-1);

//@@	if (ret == MOVIE_NOT_PLAYED) {		//couldn't find movie. try other version
//@@		name[strlen(name)-5] = MovieHires?'l':'h';				//change name
//@@		ret = RunMovie(name,!MovieHires,allow_abort,-1,-1);	//try again
//@@	}

	gr_palette_clear();		//clear out palette in case movie aborted

	digi_sample_rate = save_sample_rate;		//restore rate for game
	digi_reset(); digi_reset();

	Screen_mode = -1;		//force screen reset

	#if defined(POLY_ACC)
		PA_DFX(pa_set_write_mode (0));
	#endif

	return ret;

}
 
void MovieShowFrame (ubyte *buf,uint bufw,uint bufh,uint sx,uint sy,uint w,uint h,uint dstx,uint dsty)
{
	grs_bitmap source_bm;
	grs_point scale_pts[] = {
		{ i2f(dstx), i2f(dsty) },
		{ i2f(bufw) + i2f(dstx), i2f(bufh) + i2f(dsty) },
		{ i2f(w) + i2f(dstx), i2f(h) + i2f(dsty) }
	};

	//mprintf((0,"MovieShowFrame %d,%d  %d,%d  %d,%d  %d,%d\n",bufw,bufh,sx,sy,w,h,dstx,dsty));

	source_bm.bm_x = source_bm.bm_y = 0;
	source_bm.bm_w = source_bm.bm_rowsize = bufw;
	source_bm.bm_h = bufh;
	source_bm.bm_type = BM_LINEAR;
	source_bm.bm_flags = 0;
	source_bm.bm_data = buf;

#ifdef OGLES
	source_bm.bm_ogles_tex_id = 0;
#endif

	gr_rect(dstx, dsty, dstx + w, dsty + h);
	scale_bitmap(&source_bm, scale_pts, 0);
#ifdef OGLES
	glDeleteTextures(1, &source_bm.bm_ogles_tex_id);
#endif
	showRenderBuffer();
}

//our routine to set the pallete, called from the movie code
void MovieSetPalette(unsigned char *p, unsigned start, unsigned count)
{
	if (count == 0)
		return;

	//mprintf((0,"SetPalette p=%x, start=%d, count=%d\n",p,start,count));

	//Color 0 should be black, and we get color 255
	Assert(start>=1 && start+count-1<=254);

	//Set color 0 to be black
	gr_palette[0] = gr_palette[1] = gr_palette[2] = 0;

	//Set color 255 to be our subtitle color
	gr_palette[765] = gr_palette[766] = gr_palette[767] = 50;

	//movie libs palette into our array  
	memcpy(gr_palette+start*3,p+start*3,count*3);

	//finally set the palette in the hardware
	gr_palette_load(gr_palette);

	//MVE_SetPalette(p, start, count);
}

typedef struct bkg {
	short x, y, w, h;			// The location of the menu.
	grs_bitmap * bmp;			// The background under the menu.
} bkg;

bkg movie_bg = {0,0,0,0,NULL};

#define BOX_BORDER (MenuHires?40:20)

void show_pause_message(char *msg)
{	
	int w,h,aw;
	int x,y;

	gr_set_current_canvas(NULL);
	gr_set_curfont( SMALL_FONT );

	gr_get_string_size(msg,&w,&h,&aw);

	x = (grd_curscreen->sc_w-w)/2;
	y = (grd_curscreen->sc_h-h)/2;

	if (movie_bg.bmp) {
		gr_free_bitmap(movie_bg.bmp);
		movie_bg.bmp = NULL;
	}

	// Save the background of the display
	movie_bg.x=x; movie_bg.y=y; movie_bg.w=w; movie_bg.h=h;

	movie_bg.bmp = gr_create_bitmap( w+BOX_BORDER, h+BOX_BORDER );

	gr_bm_ubitblt(w+BOX_BORDER, h+BOX_BORDER, 0, 0, x-BOX_BORDER/2, y-BOX_BORDER/2, &(grd_curcanv->cv_bitmap), movie_bg.bmp );

	gr_setcolor(0);
	gr_rect(x-BOX_BORDER/2,y-BOX_BORDER/2,x+w+BOX_BORDER/2-1,y+h+BOX_BORDER/2-1);

	gr_set_fontcolor( 255, -1 );

	gr_ustring( 0x8000, y, msg );
}

void clear_pause_message()
{

	if (movie_bg.bmp) {

		gr_bitmap(movie_bg.x-BOX_BORDER/2, movie_bg.y-BOX_BORDER/2, movie_bg.bmp);
	
		gr_free_bitmap(movie_bg.bmp);
		movie_bg.bmp = NULL;
	}
}

FILE* open_movie_file(char *filename,int must_have);

//returns status.  see movie.h
int RunMovie(char *filename, int hires_flag, int must_have,int dx,int dy)
{
	FILE *filehndl;
	int result,aborted=0;
	int track = 0;
	int frame_num;
	int x, y;

	// Open Movie file.  If it doesn't exist, no movie, just return.

	filehndl = open_movie_file(filename,must_have);

	if (!filehndl) {
		#ifndef EDITOR
			if (must_have)
			  {
				strupr(filename);
				Error("Cannot open movie file <%s>",filename);
			  }	
			 else
			  return MOVIE_NOT_PLAYED;
		#else
			return MOVIE_NOT_PLAYED;
		#endif
	}

	MVE_memCallbacks(MPlayAlloc, MPlayFree);
	MVE_ioCallbacks(FileRead);
	MVE_sfCallbacks (MovieShowFrame);
	MVE_palCallbacks (MovieSetPalette);

	Vid_State = VID_PLAY;                           // Set to PLAY
	gr_clear_canvas(BM_XRGB(0, 0, 0));

	if (MVE_rmPrepMovie(filehndl, dx, dy, track)) {
		Int3();
		return MOVIE_NOT_PLAYED;	
	}

	frame_num = 0;

	FontHires = hires_flag;

	while((result = MVE_rmStepMovie()) == 0) {
		int mouse;

		//draw_subtitles(frame_num);

		mouse = mouse_button_down_count(MB_LEFT, &x, &y);

		// If ESCAPE pressed, then quit movie.
		if (mouse) {
			result = aborted = 1;
			break;
		}

		// If PAUSE pressed, then pause movie
		/*if (key == KEY_PAUSE) {
			MVE_rmHoldMovie();
			show_pause_message(TXT_PAUSE);
			while (!key_inkey()) ;
			clear_pause_message();
		}*/

		frame_num++;
	}

	Assert(aborted || result == MVE_ERR_EOF);		///movie should be over

	MVE_rmEndMovie();

	fclose(filehndl);                           // Close Movie File
 
	//MVE_gfxReset();

	// Restore old graphic state

	Screen_mode=-1;		//force reset of screen mode
	    
//@@   if (MenuHires) 
//@@		vga_set_mode(SM_640x480V);
//@@   else	
//@@		vga_set_mode(SM_320x200C);
	
	return (aborted?MOVIE_ABORTED:MOVIE_PLAYED_FULL);
}
		

int InitMovieBriefing ()
 {
#if defined(POLY_ACC)
    Assert(MenuHires);
	 pa_flush();
	 
	#ifdef PA_3DFX_VOODOO
    pa_begin_lfb();
    MVE_sfSVGA( 640, 480, 2048, 0, pa_get_buffer_address(0), 0, 0, NULL, 1);
	 pa_end_lfb();
	#else
    MVE_sfSVGA( 640, 480, 1280, 0, pa_get_buffer_address(0), 0, 0, NULL, 1);
	#endif

    pa_clear_buffer(0, 0);
    return 1;
#endif

	/*if (MenuHires) {
		vga_set_mode(SM_640x480V);
		if (!MVE_gfxMode(MVE_GFX_VESA_CURRENT)) {
			Int3();
			return MOVIE_NOT_PLAYED;
		}
	}
	else {
		vga_set_mode(SM_320x200C);
		if (!MVE_gfxMode(MVE_GFX_VGA_CURRENT)) {
			Int3();
			return MOVIE_NOT_PLAYED;
		}
	}*/
  return (1);
 }  
  
int FlipFlop=0;

int MyShowFrame (void)
 {
  grs_bitmap source_bm;
 
  int rw,rh,rdx,rdy;

  if (MenuHires)
   { rw=320; rh=200; rdx=280; rdy=200;}
  else
   { rw=160; rh=100; rdx=140; rdy=80;}
   
	source_bm.bm_x = source_bm.bm_y = 0;
	source_bm.bm_w = source_bm.bm_rowsize = rw;
	source_bm.bm_h = rh;
	source_bm.bm_type = BM_LINEAR;
	source_bm.bm_flags = 0;
   if (FlipFlop)
	  {
		#ifdef BUFFER_MOVIE
			memcpy (RoboBuffer[RobBufCount++],SecondVid,rw*rh);
		#endif
	  }
	else
	  {
		#ifdef BUFFER_MOVIE
			memcpy (RoboBuffer[RobBufCount++],FirstVid,rw*rh);
		#endif
	  }

  gr_bm_ubitblt(rw,rh,rdx,rdy,0,0,&source_bm,&grd_curcanv->cv_bitmap);
	 showRenderBuffer();

  FlipFlop=1-FlipFlop;
 
  return (NULL);
 }

#ifdef BUFFER_MOVIE
static fix RobBufTime=0;
#endif

void ShowRobotBuffer ()
 {
  // shows a frame from the robot buffer

#ifndef BUFFER_MOVIE
	Int3(); // Get Jason...how'd we get here?
	return;
#else
  grs_bitmap source_bm;
  int rw,rh,rdx,rdy;
  
  if (timer_get_approx_seconds()<(RobBufTime+fixdiv (F1_0,i2f(15))))
   return;	

  RobBufTime=timer_get_approx_seconds();	
  
  if (MenuHires)
   { rw=320; rh=200; rdx=280; rdy=200;}
  else
   { rw=160; rh=100; rdx=140; rdy=80;}
   
	source_bm.bm_x = source_bm.bm_y = 0;
	source_bm.bm_w = source_bm.bm_rowsize = rw;
	source_bm.bm_h = rh;
	source_bm.bm_type = BM_LINEAR;
	source_bm.bm_flags = 0;
  
	source_bm.bm_data = RoboBuffer[RobBufCount];

   gr_bm_ubitblt(rw,rh,rdx,rdy,0,0,&source_bm,&grd_curcanv->cv_bitmap);

   RobBufCount++;
   RobBufCount%=RobBufLimit;
#endif
 } 

FILE* movie_handle;
int movie_start;

//returns 1 if frame updated ok
int RotateRobot ()
{
	int err;

	if (!Digi_initialized) 			//we should fix this for full version
		return 0;

 	err = MVE_rmStepMovie();

	if (err == MVE_ERR_EOF)		//end of movie, so reset
	{
		fseek(RoboFile, movie_start, SEEK_SET);
		if (MVE_rmPrepMovie(RoboFile, (MenuHires?280:140) * f2fl(Scale_x), (MenuHires?200:80) * f2fl(Scale_y), 0)) {
			Int3();
			return 0;
		}
	}
	else if (err) {
		Int3();
		return 0;
	}

	return 1;
}

void FreeRoboBuffer (int n)
 {
  // frees the 64k frame buffers, starting with n and then working down
   
  #ifndef BUFFER_MOVIE
	n++;	//kill warning
	return;
  #else
  int i;
 
  for (i=n;i>=0;i--)
	free (RoboBuffer[i]);

  #endif 
 }


void DeInitRobotMovie()
 {
  RobBufCount=0; PlayingBuf=0;

  MVE_rmEndMovie();
   
  FreeRoboBuffer (49);	
 
  //MVE_palCallbacks (MVE_SetPalette);
  fclose(RoboFile);                           // Close Movie File
 }

void PaletteChecker (unsigned char *p,unsigned start,unsigned count)
 {
  int i;

  for (i=0;i<256;i++)
   if (p[i]!=0)
    break;

  if (i>=255 && (MVEPaletteCalls++)>0)
   return;

  //MVE_SetPalette (p,start,count);
 }


int InitRobotMovie (char *filename)
{
	
	FlipFlop=0;
	
	RobBufCount=0; PlayingBuf=0; RobBufLimit=0;
	
	if (FindArg("-nomovies"))
		return (0);
	
	mprintf ((0,"RoboFile=%s\n",filename));
	
	MVE_memCallbacks(MPlayAlloc, MPlayFree);
	MVE_ioCallbacks(FileRead);
	MVE_sfCallbacks (MovieShowFrame);
	
	RoboFile = open_movie_file(filename,1);
	
	if (RoboFile == -1) {
		FreeRoboBuffer (49);
#ifdef RELEASE
		Error("Cannot open movie file <%s>",filename);
#else
		return MOVIE_NOT_PLAYED;
#endif
	}
	
	Vid_State = VID_PLAY;
	
	if (MVE_rmPrepMovie(RoboFile, (MenuHires?280:140) * f2fl(Scale_x), (MenuHires?200:80) * f2fl(Scale_y), 0)) {
		Int3();
		FreeRoboBuffer (49);
		return 0;
	}
	
	MVE_palCallbacks (MovieSetPalette);
	RoboFilePos=fseek (RoboFile,0L,SEEK_CUR);
	mprintf ((0,"RoboFilePos=%d!\n",RoboFilePos));
	return (1);
}
/*
 *		Subtitle system code
 */

ubyte *subtitle_raw_data;

//search for next field following whitespace 
ubyte *next_field(ubyte *p)
{
	while (*p && !isspace(*p))
		p++;

	if (!*p)
		return NULL;

	while (*p && isspace(*p))
		p++;

	if (!*p)
		return NULL;

	return p;
}

void change_filename_ext( char *dest, char *src, char *ext );
void decode_text_line(char *p);

int init_subtitles(char *filename)
{
	CFILE *ifile;
	int size,read_count;
	ubyte *p;
	int have_binary = 0;

	Num_subtitles = 0;

	if (! FindArg("-subtitles"))
		return 0;

	ifile = cfopen(filename,"rb");		//try text version

	if (!ifile) {								//no text version, try binary version
		char filename2[FILENAME_LEN];
		change_filename_ext(filename2,filename,".TXB");
		ifile = cfopen(filename2,"rb");
		if (!ifile)
			return 0;
		have_binary = 1;
	}

	size = cfilelength(ifile);
   
   MALLOC (subtitle_raw_data, ubyte, size+1);

   read_count = cfread(subtitle_raw_data, 1, size, ifile);

	cfclose(ifile);

	subtitle_raw_data[size] = 0;

	if (read_count != size) {
		free(subtitle_raw_data);
		return 0;
	}

	p = subtitle_raw_data;

	while (p && p < subtitle_raw_data+size) {
		char *endp;

		endp = strchr(p,'\n'); 
		if (endp) {
			if (endp[-1] == '\r')
				endp[-1] = 0;		//handle 0d0a pair
			*endp = 0;			//string termintor
		}

		if (have_binary)
			decode_text_line(p);

		if (*p != ';') {
			Subtitles[Num_subtitles].first_frame = atoi(p);
			p = next_field(p); if (!p) continue;
			Subtitles[Num_subtitles].last_frame = atoi(p);
			p = next_field(p); if (!p) continue;
			Subtitles[Num_subtitles].msg = p;

			Assert(Num_subtitles==0 || Subtitles[Num_subtitles].first_frame >= Subtitles[Num_subtitles-1].first_frame);
			Assert(Subtitles[Num_subtitles].last_frame >= Subtitles[Num_subtitles].first_frame);

			Num_subtitles++;
		}

		p = endp+1;

	}

	return 1;

}

void close_subtitles()
{
	if (subtitle_raw_data)
		free(subtitle_raw_data);
	subtitle_raw_data = NULL;
	Num_subtitles = 0;
}

#define MAX_ACTIVE_SUBTITLES 3

//draw the subtitles for this frame
void draw_subtitles(int frame_num)
{
	static int active_subtitles[MAX_ACTIVE_SUBTITLES];
	static int num_active_subtitles,next_subtitle,line_spacing;
	int t,y;
	int must_erase=0;

	if (frame_num == 0) {
		num_active_subtitles = 0;
		next_subtitle = 0;
		gr_set_curfont( GAME_FONT );
		line_spacing = grd_curcanv->cv_font->ft_h + (grd_curcanv->cv_font->ft_h >> 2);
		gr_set_fontcolor(255,-1);
	}

	//get rid of any subtitles that have expired
	for (t=0;t<num_active_subtitles;)
		if (frame_num > Subtitles[active_subtitles[t]].last_frame) {
			int t2;
			for (t2=t;t2<num_active_subtitles-1;t2++)
				active_subtitles[t2] = active_subtitles[t2+1];
			num_active_subtitles--;
			must_erase = 1;
		}
		else
			t++;

	//get any subtitles new for this frame 
	while (next_subtitle < Num_subtitles && frame_num >= Subtitles[next_subtitle].first_frame) {
		if (num_active_subtitles >= MAX_ACTIVE_SUBTITLES)
			Error("Too many active subtitles!");
		active_subtitles[num_active_subtitles++] = next_subtitle;
		next_subtitle++;
	}

	//find y coordinate for first line of subtitles
	y = grd_curcanv->cv_bitmap.bm_h-((line_spacing+1)*MAX_ACTIVE_SUBTITLES+2);

	//erase old subtitles if necessary
	if (must_erase) {
		gr_setcolor(0);
		gr_rect(0,y,grd_curcanv->cv_bitmap.bm_w-1,grd_curcanv->cv_bitmap.bm_h-1);
	}

	//now draw the current subtitles
	for (t=0;t<num_active_subtitles;t++)
		if (active_subtitles[t] != -1) {
			gr_string(0x8000,y,Subtitles[active_subtitles[t]].msg);
			y += line_spacing+1;
		}
}

typedef struct {
	char name[FILENAME_LEN];
	int offset,len;
} ml_entry;

#define MLF_ON_CD		1

typedef struct {
	char		name[PATH_MAX];	//[FILENAME_LEN];
	int		n_movies;
	ubyte		flags,pad[3];
	ml_entry	movies[];
} movielib;

#define MAX_MOVIES_PER_LIB		50		//determines size of malloc

movielib *init_new_movie_lib(char *filename,FILE *fp)
{
	int nfiles,offset;
	int i,n;
	movielib *table;

	//read movie file header

	fread(&nfiles,4,1,fp);		//get number of files

	table = malloc(sizeof(*table) + sizeof(ml_entry)*nfiles);

	strcpy(table->name,filename);
	table->n_movies = nfiles;

	offset = 4+4+nfiles*(13+4);	//id + nfiles + nfiles * (filename + size)

	for (i=0;i<nfiles;i++) {
		int len;

		n = fread( table->movies[i].name, 13, 1, fp );
		if ( n != 1 )
			break;		//end of file (probably)

		n = fread( &len, 4, 1, fp );
		if ( n != 1 )
			Error("error reading movie library <%s>",filename);

		table->movies[i].len = INTEL_INT(len);
		table->movies[i].offset = offset;

		offset += table->movies[i].len;

	}

	fclose(fp);

	table->flags = 0;

	return table;

}

movielib *init_old_movie_lib(char *filename,FILE *fp)
{
	int nfiles,size;
	int i;
	movielib *table,*table2;

	nfiles = 0;

	//allocate big table
	table = malloc(sizeof(*table) + sizeof(ml_entry)*MAX_MOVIES_PER_LIB);

	while( 1 ) {
		int len;

		i = fread( table->movies[nfiles].name, 13, 1, fp );
		if ( i != 1 )
			break;		//end of file (probably)

		i = fread( &len, 4, 1, fp );
		if ( i != 1 )
			Error("error reading movie library <%s>",filename);

		table->movies[nfiles].len = INTEL_INT(len);
		table->movies[nfiles].offset = ftell( fp );

		fseek( fp, INTEL_INT(len), SEEK_CUR );		//skip data

		nfiles++;
	}

	//allocate correct-sized table
	size = sizeof(*table) + sizeof(ml_entry)*nfiles;
	table2 = malloc(size);
	memcpy(table2,table,size);
	free(table);
	table = table2;

	strcpy(table->name,filename);

	table->n_movies = nfiles;

	fclose(fp);

	table->flags = 0;

	return table;

}

//find the specified movie library, and read in list of movies in it   
movielib *init_movie_lib(char *filename)
{
	//note: this based on cfile_init_hogfile()

	char id[4];
	char path[PATH_MAX];
	FILE * fp;

	strlwr(filename);
#ifdef ANDROID_NDK
	AAsset* asset = AAssetManager_open(Asset_manager, filename, AASSET_MODE_BUFFER);
	if (!asset) {
		fp = NULL;
	} else {
		fp = funopen(asset, android_read, android_write, android_seek, android_close);
		strncpy(path, filename, PATH_MAX);
	}
#else
	sprintf(path, "%s/%s", Resource_path, filename);
	fp = fopen( path, "rb" );
#endif
	if ( fp == NULL ) 
		return NULL;

	fread( id, 4, 1, fp );
	if ( !strncmp( id, "DMVL", 4 ) )
		return init_new_movie_lib(path,fp);
	else if ( !strncmp( id, "DHF", 3 ) ) {
		fseek(fp,-1,SEEK_CUR);		//old file had 3 char id
		return init_old_movie_lib(path,fp);
	}
	else {
		fclose(fp);
		return NULL;
	}
}

#ifdef D2_OEM
char *movielib_files[] = {"intro-l.mvl","other-l.mvl","robots-l.mvl","oem-l.mvl"};
#else
char *movielib_files[] = {"intro-l.mvl","other-l.mvl","robots-l.mvl"};
#endif

#define N_BUILTIN_MOVIE_LIBS (sizeof(movielib_files)/sizeof(*movielib_files))
#define N_MOVIE_LIBS (N_BUILTIN_MOVIE_LIBS+1)
#define EXTRA_ROBOT_LIB N_BUILTIN_MOVIE_LIBS
movielib *movie_libs[N_MOVIE_LIBS];

void close_movie(int i)
{
	if (movie_libs[i])
		free(movie_libs[i]);
}

void close_movies()
{
	int i;

	for (i=0;i<N_MOVIE_LIBS;i++)
		close_movie(i);
}

#include "gamepal.h"

extern char CDROM_dir[];
extern int MenuHiresAvailable;

extern ubyte last_palette_for_color_fonts[];

extern int force_rb_register;

//ask user to put the D2 CD in.
//returns -1 if ESC pressed, 0 if OK chosen
//CD may not have been inserted
int request_cd()
{
	ubyte save_pal[256*3];
	grs_canvas *save_canv,*tcanv;
	int ret,was_faded=gr_palette_faded_out;

	gr_palette_clear();

	save_canv = grd_curcanv;
	tcanv = gr_create_canvas(grd_curcanv->cv_w,grd_curcanv->cv_h);

	gr_set_current_canvas(tcanv);
	gr_ubitmap(0,0,&save_canv->cv_bitmap);
	gr_set_current_canvas(save_canv);

	gr_clear_canvas(BM_XRGB(0,0,0));
	
	memcpy(save_pal,gr_palette,sizeof(save_pal));

	memcpy(gr_palette,last_palette_for_color_fonts,sizeof(gr_palette));

try_again:;

	ret = nm_messagebox( "CD ERROR", 1, "Ok", "Please insert your Descent II CD");

	if (ret == -1) {
		int ret2;

		ret2 = nm_messagebox( "CD ERROR", 2, "Try Again", "Leave Game", "You must insert your\nDescent II CD to Continue");

		if (ret2 == -1 || ret2 == 0)
			goto try_again;
	}

	force_rb_register = 1;	//disc has changed; force register new CD    
	
	gr_palette_clear();

	memcpy(gr_palette,save_pal,sizeof(save_pal));
	
	gr_ubitmap(0,0,&tcanv->cv_bitmap);

	if (!was_faded)
		gr_palette_load(gr_palette);

	gr_free_canvas(tcanv);

	return ret;
}

//do we have the robot movies available
int robot_movies=0;	//0 means none, 1 means lowres, 2 means hires

void init_movie(char *filename,int libnum,int is_robots,int required)
{
	int high_res;
	char temp[13];
	
	strncpy(temp, filename, 13);

	#ifndef RELEASE
	if (FindArg("-nomovies")) {
		movie_libs[libnum] = NULL;
		return;
	}
	#endif

	//for robots, load highres versions if highres menus set
	if (is_robots)
		high_res = MenuHiresAvailable;
	else
		high_res = MovieHires;

	if (high_res)
		strchr(temp,'.')[-1] = 'h';
	filename = temp;

try_again:;

	if ((movie_libs[libnum] = init_movie_lib(filename)) == NULL) {
		char name2[100];
		
		strcpy(name2,CDROM_dir);
		strcat(name2,filename);
		movie_libs[libnum] = init_movie_lib(name2);

		if (movie_libs[libnum] != NULL)
			movie_libs[libnum]->flags |= MLF_ON_CD;
		else {
			if (required) {
				#if defined(RELEASE) && !defined(D2_OEM)		//allow no movies if not release
					strupr(filename);
					Error("Cannot open movie file <%s>",filename);
				#endif
			}
			#if defined(D2_OEM)		//if couldn't get higres, try low
			if (is_robots == 1) {	//first try, try again with lowres
				strchr(filename,'.')[-1] = 'l';
				high_res = 0;
				is_robots++;
				goto try_again;
			}
			else if (is_robots == 2) {		//failed twice. bail with error
				strupr(filename);
				Error("Cannot open movie file <%s>",filename);
			}
			#endif
		}
	}

	if (is_robots && movie_libs[libnum]!=NULL)
		robot_movies = high_res?2:1;
}

//find and initialize the movie libraries
void init_movies()
{
	int i;
	int is_robots;

	for (i=0;i<N_BUILTIN_MOVIE_LIBS;i++) {

		if (!strncasecmp(movielib_files[i],"robot",5))
			is_robots = 1;
		else
			is_robots = 0;

		init_movie(movielib_files[i],i,is_robots,1);
	}

	movie_libs[EXTRA_ROBOT_LIB] = NULL;

	atexit(close_movies);

}

void init_extra_robot_movie(char *filename)
{
	close_movie(EXTRA_ROBOT_LIB);
	init_movie(filename,EXTRA_ROBOT_LIB,1,0);
}

//looks through a movie library for a movie file
//returns filehandle, with fileposition at movie, or -1 if can't find
FILE* search_movie_lib(movielib *lib,char *filename,int must_have)
{
	int i;
	FILE *filehandle;

	if (lib == NULL)
		return NULL;

	for (i=0;i<lib->n_movies;i++)
		if (!strcasecmp(filename,lib->movies[i].name)) {	//found the movie in a library
			int from_cd;

			from_cd = (lib->flags & MLF_ON_CD);

			if (from_cd)
				songs_stop_redbook();		//ready to read from CD

			do {		//keep trying until we get the file handle

#ifdef ANDROID_NDK
				AAsset* asset = AAssetManager_open(Asset_manager, lib->name, AASSET_MODE_BUFFER);
				if (!asset) {
					movie_handle = filehandle = NULL;
				} else {
					movie_handle = filehandle = funopen(asset, android_read, android_write, android_seek, android_close);
				}
#else
				movie_handle = filehandle = fopen(lib->name, "rb");
#endif

				if (must_have && from_cd && filehandle == NULL) {		//didn't get file!

					if (request_cd() == -1)		//ESC from requester
						break;						//bail from here. will get error later
				}

			} while (must_have && from_cd && filehandle == NULL);

			if (filehandle != NULL)
				fseek(filehandle,(movie_start=lib->movies[i].offset),SEEK_SET);

			return filehandle;
		}

	return NULL;
}

//returns file handle
FILE* open_movie_file(char *filename,int must_have)
{
	FILE *filehandle;
	int i;

	for (i=0;i<N_MOVIE_LIBS;i++) {

		if ((filehandle = search_movie_lib(movie_libs[i],filename,must_have)))
			return filehandle;
	}

	return NULL;		//couldn't find it
}

//sets the file position to the start of this already-open file
int reset_movie_file(int handle)
{
	Assert(handle == movie_handle);

	fseek(handle,movie_start,SEEK_SET);

	return 0;		//everything is cool
}


