/*____________________________________________________________________
|
| File: main.cpp
|
| Description: Main module in program.
|
| Functions:  Program_Get_User_Preferences
|             Program_Init
|							 Init_Graphics
|								Set_Mouse_Cursor
|             Program_Run
|							 Init_Render_State
|             Program_Free
|             Program_Immediate_Key_Handler
|
| (C) Copyright 2013 Abonvita Software LLC.
| Licensed under the GX Toolkit License, Version 1.0.
|___________________________________________________________________*/

#define _MAIN_

/*___________________
|
| Include Files
|__________________*/

#include <first_header.h>
#include "dp.h"
#include "..\Framework\win_support.h"
#include <rom8x8.h>

#include "main.h"
#include "position.h"

/*___________________
|
| Type definitions
|__________________*/

typedef struct {
	unsigned resolution;
	unsigned bitdepth;
} UserPreferences;

/*___________________
|
| Function Prototypes
|__________________*/

static int Init_Graphics(unsigned resolution, unsigned bitdepth, unsigned stencildepth, int *generate_keypress_events);
static void Set_Mouse_Cursor();
static void Init_Render_State();
static gx3dMotion *Load_Motion(gx3dMotionSkeleton *mskeleton, char *filename, int fps, gx3dMotionMetadataRequest *metadata_requested, int num_metadata_requested, bool load_all_metadata);

/*___________________
|
| Constants
|__________________*/

#define MAX_VRAM_PAGES  2
#define GRAPHICS_RESOLUTION  \
  (                          \
    gxRESOLUTION_640x480   | \
    gxRESOLUTION_800x600   | \
    gxRESOLUTION_1024x768  | \
    gxRESOLUTION_1152x864  | \
    gxRESOLUTION_1280x960  | \
    gxRESOLUTION_1400x1050 | \
    gxRESOLUTION_1440x1080 | \
    gxRESOLUTION_1600x1200 | \
    gxRESOLUTION_1152x720  | \
    gxRESOLUTION_1280x800  | \
    gxRESOLUTION_1440x900  | \
    gxRESOLUTION_1680x1050 | \
    gxRESOLUTION_1920x1200 | \
    gxRESOLUTION_2048x1280 | \
    gxRESOLUTION_1280x720  | \
    gxRESOLUTION_1600x900  | \
    gxRESOLUTION_1920x1080   \
  )
#define GRAPHICS_STENCILDEPTH 0
#define GRAPHICS_BITDEPTH (gxBITDEPTH_24 | gxBITDEPTH_32)

#define AUTO_TRACKING    1
#define NO_AUTO_TRACKING 0
#define SCREENSHOT_FILENAME "screenshots\\screen"

/*____________________________________________________________________
|
| Function: Program_Get_User_Preferences
|
| Input: Called from CMainFrame::Init
| Output: Allows program to popup dialog boxes, etc. to get any user
|   preferences such as screen resolution.  Returns preferences via a
|   pointer.  Returns true on success, else false to quit the program.
|___________________________________________________________________*/

int Program_Get_User_Preferences(void **preferences)
{
	static UserPreferences user_preferences;

	if (gxGetUserFormat(GRAPHICS_DRIVER, GRAPHICS_RESOLUTION, GRAPHICS_BITDEPTH, &user_preferences.resolution, &user_preferences.bitdepth)) {
		*preferences = (void *)&user_preferences;
		return (1);
	}
	else
		return (0);
}

/*____________________________________________________________________
|
| Function: Program_Init
|
| Input: Called from CMainFrame::Start_Program_Thread()
| Output: Starts graphics mode.  Returns # of user pages available if
|       successful, else 0.
|___________________________________________________________________*/

int Program_Init(void *preferences, int *generate_keypress_events)
{
	UserPreferences *user_preferences = (UserPreferences *)preferences;
	int initialized = FALSE;

	if (user_preferences)
		initialized = Init_Graphics(user_preferences->resolution, user_preferences->bitdepth, GRAPHICS_STENCILDEPTH, generate_keypress_events);

	return (initialized);
}

/*____________________________________________________________________
|
| Function: Init_Graphics
|
| Input: Called from Program_Init()
| Output: Starts graphics mode.  Returns # of user pages available if
|       successful, else 0.
|___________________________________________________________________*/

static int Init_Graphics(unsigned resolution, unsigned bitdepth, unsigned stencildepth, int *generate_keypress_events)
{
	int num_pages;
	byte *font_data;
	unsigned font_size;

	/*____________________________________________________________________
	|
	| Init globals
	|___________________________________________________________________*/

	Pgm_num_pages = 0;
	Pgm_system_font = NULL;

	/*____________________________________________________________________
	|
	| Start graphics mode and event processing
	|___________________________________________________________________*/

	font_data = font_data_rom8x8;
	font_size = sizeof(font_data_rom8x8);

	// Start graphics mode                                      
	num_pages = gxStartGraphics(resolution, bitdepth, stencildepth, MAX_VRAM_PAGES, GRAPHICS_DRIVER);
	if (num_pages == MAX_VRAM_PAGES) {
		// Init system, drawing fonts 
		Pgm_system_font = gxLoadFontData(gxFONT_TYPE_GX, font_data, font_size);
		// Make system font the default drawing font 
		gxSetFont(Pgm_system_font);

		// Start event processing
		evStartEvents(evTYPE_MOUSE_LEFT_PRESS | evTYPE_MOUSE_RIGHT_PRESS |
			evTYPE_MOUSE_LEFT_RELEASE | evTYPE_MOUSE_RIGHT_RELEASE |
			evTYPE_MOUSE_WHEEL_BACKWARD | evTYPE_MOUSE_WHEEL_FORWARD |
			//                   evTYPE_KEY_PRESS | 
			evTYPE_RAW_KEY_PRESS | evTYPE_RAW_KEY_RELEASE,
			AUTO_TRACKING, EVENT_DRIVER);
		*generate_keypress_events = FALSE;  // true if using evTYPE_KEY_PRESS in the above mask

		// Set a custom mouse cursor
		Set_Mouse_Cursor();

		// Set globals
		Pgm_num_pages = num_pages;
	}

	return (Pgm_num_pages);
}

/*____________________________________________________________________
|
| Function: Set_Mouse_Cursor
|
| Input: Called from Init_Graphics()
| Output: Sets default mouse cursor.
|___________________________________________________________________*/

static void Set_Mouse_Cursor()
{
	gxColor fc, bc;

	// Set cursor to a medium sized red arrow
	fc.r = 255;
	fc.g = 0;
	fc.b = 0;
	fc.a = 0;
	bc.r = 1;
	bc.g = 1;
	bc.b = 1;
	bc.a = 0;
	msSetCursor(msCURSOR_MEDIUM_ARROW, fc, bc);
}

/*____________________________________________________________________
|
| Function: Program_Run
|
| Input: Called from Program_Thread()
| Output: Runs program in the current video mode.  Begins with mouse
|   hidden.
|___________________________________________________________________*/

void Program_Run()
{
	int quit;
	evEvent event;
	gx3dDriverInfo dinfo;
	gxColor color;
	char str[256];
	unsigned ani_time;
	bool take_screenshot;
	bool play_animation;

	gx3dObject *obj_tree, *obj_tree2, *obj_skydome, *obj_clouddome, *obj_ghost, *obj_billboard_tree;
	gx3dMatrix m, m1, m2, m3, m4, m5, g, s, mo;
	gx3dColor color3d_white = { 1, 1, 1, 0 };
	gx3dColor color3d_dim = { 0.1f, 0.1f, 0.1f };
	gx3dColor color3d_black = { 0, 0, 0, 0 };
	gx3dColor color3d_darkgray = { 0.3f, 0.3f, 0.3f, 0 };
	gx3dColor color3d_gray = { 0.5f, 0.5f, 0.5f, 0 };
	gx3dMaterialData material_default = {
		{ 1, 1, 1, 1 }, // ambient color
		{ 1, 1, 1, 1 }, // diffuse color
		{ 1, 1, 1, 1 }, // specular color
		{ 0, 0, 0, 0 }, // emissive color
		10              // specular sharpness (0=disabled, 0.01=sharp, 10=diffused)
	};

	gx3dMotionSkeleton *mskeleton = 0;
	// // How to use C++ print outupt 
	// string mystr;

	//mystr = "SUPER!";
	//char ss[256];
	//strcpy (ss, mystr.c_str());
	//debug_WriteFile (ss);


	/*____________________________________________________________________
	|
	| Print info about graphics driver to debug file.
	|___________________________________________________________________*/

	gx3d_GetDriverInfo(&dinfo);
	debug_WriteFile("_______________ Device Info ______________");
	sprintf(str, "max texture size: %dx%d", dinfo.max_texture_dx, dinfo.max_texture_dy);
	debug_WriteFile(str);
	sprintf(str, "max active lights: %d", dinfo.max_active_lights);
	debug_WriteFile(str);
	sprintf(str, "max user clip planes: %d", dinfo.max_user_clip_planes);
	debug_WriteFile(str);
	sprintf(str, "max simultaneous texture stages: %d", dinfo.max_simultaneous_texture_stages);
	debug_WriteFile(str);
	sprintf(str, "max texture stages: %d", dinfo.max_texture_stages);
	debug_WriteFile(str);
	sprintf(str, "max texture repeat: %d", dinfo.max_texture_repeat);
	debug_WriteFile(str);
	debug_WriteFile("__________________________________________");

	/*____________________________________________________________________
	|
	| Initialize the sound library
	|___________________________________________________________________*/

	snd_Init(22, 16, 2, 1, 1);
	snd_SetListenerDistanceFactorToFeet(snd_3D_APPLY_NOW);

	Sound  s_song, s_footstep, s_die, s_yeah, s_boom, s_over;

	s_song = snd_LoadSound("wav\\background.wav", snd_CONTROL_VOLUME, 0);

	s_footstep = snd_LoadSound("wav\\footstep.wav", snd_CONTROL_VOLUME, 0);
	s_die = snd_LoadSound("wav\\die.wav", snd_CONTROL_VOLUME, 0);
	s_yeah = snd_LoadSound("wav\\ping.wav", snd_CONTROL_VOLUME, 0);
	s_boom = snd_LoadSound("wav\\boom.wav", snd_CONTROL_VOLUME, 0);
	s_over = snd_LoadSound("wav\\gameover.wav", snd_CONTROL_VOLUME, 0);
	/*____________________________________________________________________
	|
	| Initialize the graphics state
	|___________________________________________________________________*/

	// Set 2d graphics state
	Pgm_screen.xleft = 0;
	Pgm_screen.ytop = 0;
	Pgm_screen.xright = gxGetScreenWidth() - 1;
	Pgm_screen.ybottom = gxGetScreenHeight() - 1;
	gxSetWindow(&Pgm_screen);
	gxSetClip(&Pgm_screen);
	gxSetClipping(FALSE);

	// Set the 3D viewport
	gx3d_SetViewport(&Pgm_screen);
	// Init other 3D stuff
	Init_Render_State();

	/*____________________________________________________________________
	|
	| Init support routines
	|___________________________________________________________________*/

	gx3dVector heading, position;

	// Set starting camera position
	position.x = 0;
	position.y = 15;
	position.z = -130;
	// Set starting camera view direction (heading)
	heading.x = 0;  // {0,0,1} for cubic environment mapping to work correctly
	heading.y = 0;
	heading.z = 1;
	Position_Init(&position, &heading, RUN_SPEED);

	/*____________________________________________________________________
	|
	| Init 3D graphics
	|___________________________________________________________________*/

	// Set projection matrix
	float fov = 60; // degrees field of view
	float near_plane = 0.1f;
	float far_plane = 1000;
	gx3d_SetProjectionMatrix(fov, near_plane, far_plane);

	gx3d_SetFillMode(gx3d_FILL_MODE_GOURAUD_SHADED);

	// Clear the 3D viewport to all black
	color.r = 0;
	color.g = 0;
	color.b = 0;
	color.a = 0;

	/*____________________________________________________________________
	|
	| Load 3D models
	|___________________________________________________________________*/

	gx3dParticleSystem psys_fire = Script_ParticleSystem_Create("fire.gxps");
	gx3dParticleSystem psys_power = Script_ParticleSystem_Create("power.gxps");

	//Load character model
	gx3dObject *obj_character;
	gx3d_ReadLWO2File("Objects\\tifa.lwo", &obj_character, gx3d_VERTEXFORMAT_TEXCOORDS | gx3d_VERTEXFORMAT_WEIGHTS, gx3d_MERGE_DUPLICATE_VERTICES | gx3d_SMOOTH_DISCONTINUOUS_VERTICES | gx3d_DONT_LOAD_TEXTURES);
	gx3dTexture tex_character = gx3d_InitTexture_File("Objects\\tifa_tex_d512.bmp", "Objects\\tifa_tex_d512_fa.bmp", 0);
	mskeleton = gx3d_MotionSkeleton_Read_LWS_File("Objects\\tifa movement.lws");
	// Load a motion
	gx3dMotion *motion1 = 0;
	motion1 = Load_Motion(mskeleton, "Objects\\tifa_step.lws", 30, 0, 0, false);

	// Create a blend tree                                                
	gx3dBlendNode *bnode1;
	gx3dBlendTree *btree1;
	bnode1 = gx3d_BlendNode_Init(mskeleton, gx3d_BLENDNODE_TYPE_SINGLE); // create single input blending node (doesn't blend, just reads in)
	gx3d_Motion_Set_Output(motion1, bnode1, gx3d_BLENDNODE_TRACK_0);  // set output of animation to blending node (track 0)

	btree1 = gx3d_BlendTree_Init(mskeleton);            // create blendtree
	gx3d_BlendTree_Add_Node(btree1, bnode1);             // add blending node to the tree
	gx3d_BlendTree_Set_Output(btree1, obj_character->layer);  // set output of tree to a model object layer (containing vertices to be animated)

	// Load a 3D model																								
	gx3d_ReadLWO2File("Objects\\tree2.lwo", &obj_tree, gx3d_VERTEXFORMAT_DEFAULT, gx3d_DONT_LOAD_TEXTURES);
	// Load the same model but make sure mipmapping of the texture is turned off
	gx3d_ReadLWO2File("Objects\\tree2.lwo", &obj_tree2, gx3d_VERTEXFORMAT_DEFAULT, gx3d_DONT_LOAD_TEXTURES | gx3d_DONT_GENERATE_MIPMAPS);
	gx3dTexture tex_tree = gx3d_InitTexture_File("Objects\\Images\\leaves.bmp", 0, 0);
	gx3dTexture tex_bark = gx3d_InitTexture_File("Objects\\Images\\bark_texture.bmp", 0, 0);

	//Sky dome
	gx3d_ReadLWO2File("Objects\\skydome.lwo", &obj_skydome, gx3d_VERTEXFORMAT_DEFAULT, gx3d_DONT_LOAD_TEXTURES);
	gx3dTexture tex_skydome = gx3d_InitTexture_File("Objects\\Images\\bright_sky_d128.bmp", 0, 0);

	//Cloud dome
	gx3d_ReadLWO2File("Objects\\clouddome.lwo", &obj_clouddome, gx3d_VERTEXFORMAT_DEFAULT, gx3d_DONT_LOAD_TEXTURES);
	gx3dTexture tex_clouddome = gx3d_InitTexture_File("Objects\\purple_cloud.bmp", 0, 0);

	//Ghost
	gx3d_ReadLWO2File("Objects\\billboard_ghost.lwo", &obj_ghost, gx3d_VERTEXFORMAT_DEFAULT, gx3d_DONT_LOAD_TEXTURES);
	gx3dTexture tex_red_ghost = gx3d_InitTexture_File("Objects\\Images\\ghost_die.bmp", "Objects\\Images\\ghost_fa.bmp", 0);

	//Hit
	gx3dObject *obj_hit;
	gx3d_ReadLWO2File("Objects\\hit.lwo", &obj_hit, gx3d_VERTEXFORMAT_DEFAULT, gx3d_DONT_LOAD_TEXTURES);
	gx3dTexture tex_hit = gx3d_InitTexture_File("Objects\\yeah.bmp", "Objects\\yeah_fa.bmp", 0);
	gx3dTexture tex_boom = gx3d_InitTexture_File("Objects\\boom.bmp", "Objects\\boom_fa.bmp", 0);

	//Ground
	gx3dObject *obj_ground;
	gx3d_ReadLWO2File("Objects\\ground.lwo", &obj_ground, gx3d_VERTEXFORMAT_DEFAULT, gx3d_DONT_LOAD_TEXTURES);
	gx3dTexture tex_ground = gx3d_InitTexture_File("Objects\\sand.bmp", 0, 0);

	//Start screen
	gx3dObject *obj_start;
	gx3d_ReadLWO2File("Objects\\game.lwo", &obj_start, gx3d_VERTEXFORMAT_DEFAULT, gx3d_DONT_LOAD_TEXTURES);
	gx3dTexture tex_start = gx3d_InitTexture_File("Objects\\startscreen.bmp", 0, 0);
	gx3dTexture tex_over = gx3d_InitTexture_File("Objects\\gameover.bmp", 0, 0);

	//flower
	gx3dObject *obj_flower;
	gx3d_ReadLWO2File("Objects\\flower.lwo", &obj_flower, gx3d_VERTEXFORMAT_DEFAULT, gx3d_DONT_LOAD_TEXTURES);
	gx3dTexture tex_flower = gx3d_InitTexture_File("Objects\\flower.bmp", "Objects\\flower_fa.bmp", 0);

	//grass
	gx3dObject *obj_grass;
	gx3d_ReadLWO2File("Objects\\grass.lwo", &obj_grass, gx3d_VERTEXFORMAT_DEFAULT, gx3d_DONT_LOAD_TEXTURES);
	gx3dTexture tex_grass = gx3d_InitTexture_File("Objects\\grass.bmp", "Objects\\grass_fa.bmp", 0);

	//power
	gx3dObject *obj_power;
	gx3d_ReadLWO2File("Objects\\power.lwo", &obj_power, gx3d_VERTEXFORMAT_DEFAULT, gx3d_DONT_LOAD_TEXTURES);
	gx3dTexture tex_purple = gx3d_InitTexture_File("Objects\\tex_purple.bmp", 0, 0);

	//explosion
	gx3dObject *obj_explosion;
	gx3d_ReadLWO2File("Objects\\explosion.lwo", &obj_explosion, gx3d_VERTEXFORMAT_DEFAULT, gx3d_DONT_LOAD_TEXTURES);
	gx3dTexture tex_explosion = gx3d_InitTexture_File("Objects\\tex_fire.bmp", 0, 0);

	//skull
	gx3dObject *obj_die;
	gx3d_ReadLWO2File("Objects\\die.lwo", &obj_die, gx3d_VERTEXFORMAT_DEFAULT, gx3d_DONT_LOAD_TEXTURES);
	gx3dTexture tex_die = gx3d_InitTexture_File("Objects\\tex_die.bmp", "Objects\\die_fa.bmp", 0);

	//mountain
	gx3dObject *obj_mountain;
	gx3d_ReadLWO2File("Objects\\mountain.lwo", &obj_mountain, gx3d_VERTEXFORMAT_DEFAULT, gx3d_DONT_LOAD_TEXTURES);
	gx3dTexture tex_mountain = gx3d_InitTexture_File("Objects\\tex_mountain.bmp", 0, 0);

	//tall tree
	gx3dObject *obj_talltree;
	gx3d_ReadLWO2File("Objects\\ptree6.lwo", &obj_talltree, gx3d_VERTEXFORMAT_DEFAULT, gx3d_DONT_LOAD_TEXTURES);
	// Load the texture (with mipmaps)
	gx3dTexture tex_talltree = gx3d_InitTexture_File("Objects\\Images\\ptree_d512.bmp", "Objects\\Images\\ptree_d512_fa.bmp", 0);

	//Power	
#define NUM_POWER 15
	gx3dVector powerPosition[NUM_POWER];
	float powerSpeed[NUM_POWER];
	int height[NUM_POWER];
	int num_die = 0;
	gx3dSphere powerSphere[NUM_POWER];
	boolean powerDraw[NUM_POWER];
   
	height[0] = 40;

	const int MAX_HIT = 1;
	gx3dVector hitPosition[MAX_HIT];
	int hitTimer[MAX_HIT];
	int hitIndex = 0;

	for (int i = 1; i < NUM_POWER; i++) {
		height[i] = height[i-1] + 50;
	}

	for (int i = 0; i < NUM_POWER; i++) {
		powerPosition[i].x = (int)(rand() % 70 - 30);
		powerPosition[i].y = height[i];
		powerPosition[i].z = (int)(rand() % 40 - 20);
		powerSpeed[i] = ((float)rand()) / ((float)RAND_MAX)*0.10f + 0.05f;
		powerDraw[i] = true;
	}

	//Explosion
#define NUM_EXPLOSION 10
	gx3dVector exPosition[NUM_EXPLOSION];
	float exSpeed[NUM_EXPLOSION];
	int exheight[NUM_EXPLOSION];
	gx3dSphere exSphere[NUM_EXPLOSION];
	boolean exDraw[NUM_EXPLOSION];

	exheight[0] = 50;

	const int exMAX_HIT = 1;
	gx3dVector exhitPosition[exMAX_HIT];
	int exhitTimer[exMAX_HIT];
	int exhitIndex = 0;

	for (int i = 1; i < NUM_EXPLOSION; i++) {
		exheight[i] = exheight[i - 1] + 40;
	}

	for (int i = 0; i < NUM_EXPLOSION; i++) {
		exPosition[i].x = (int)(rand() % 70 - 30);
		exPosition[i].y = exheight[i];
		exPosition[i].z = (int)(rand() % 40 - 20);
		exSpeed[i] = ((float)rand()) / ((float)RAND_MAX)*0.2f + 0.1f;
		exDraw[i] = true;
	}


	bool game_over = false;
	int xmove = 0;
	int ymove = 0;
	int zmove = 0;
	int facing = 180;
	gx3dSphere characterSphere;


	/*____________________________________________________________________
	|
	| create lights
	|___________________________________________________________________*/

	gx3dLight dir_light;
	gx3dLightData light_data;
	light_data.light_type = gx3d_LIGHT_TYPE_DIRECTION;
	light_data.direction.diffuse_color.r = 1;
	light_data.direction.diffuse_color.g = 1;
	light_data.direction.diffuse_color.b = 1;
	light_data.direction.diffuse_color.a = 0;
	light_data.direction.specular_color.r = 1;
	light_data.direction.specular_color.g = 1;
	light_data.direction.specular_color.b = 1;
	light_data.direction.specular_color.a = 0;
	light_data.direction.ambient_color.r = 0;
	light_data.direction.ambient_color.g = 0;
	light_data.direction.ambient_color.b = 0;
	light_data.direction.ambient_color.a = 0;
	light_data.direction.dst.x = -1;
	light_data.direction.dst.y = -1;
	light_data.direction.dst.z = 0;
	dir_light = gx3d_InitLight(&light_data);

	gx3dLight point_light1;
	light_data.light_type = gx3d_LIGHT_TYPE_POINT;
	light_data.point.diffuse_color.r = 1;  // red light
	light_data.point.diffuse_color.g = 1;
	light_data.point.diffuse_color.b = 1;
	light_data.point.diffuse_color.a = 0;
	light_data.point.specular_color.r = 1;
	light_data.point.specular_color.g = 1;
	light_data.point.specular_color.b = 1;
	light_data.point.specular_color.a = 0;
	light_data.point.ambient_color.r = 0;  // ambient turned offf
	light_data.point.ambient_color.g = 0;
	light_data.point.ambient_color.b = 0;
	light_data.point.ambient_color.a = 0;
	light_data.point.src.x = 10;
	light_data.point.src.y = 20;
	light_data.point.src.z = 0;
	light_data.point.range = 200;
	light_data.point.constant_attenuation = 0;
	light_data.point.linear_attenuation = 0.1;
	light_data.point.quadratic_attenuation = 0;
	point_light1 = gx3d_InitLight(&light_data);

	gx3dVector light_position = { 10, 20, 0 }, xlight_position;
	float angle = 0;

	/*____________________________________________________________________
	|
	| Flush input queue
	|___________________________________________________________________*/

	int move_x, move_y;	// mouse movement counters

	// Flush input queue
	evFlushEvents();
	// Zero mouse movement counters
	msGetMouseMovement(&move_x, &move_y);  // call this here so the next call will get movement that has occurred since it was called here                                    
	// Hide mouse cursor
	msHideMouse();

	/*____________________________________________________________________
	|
	| Main game loop
	|___________________________________________________________________*/

	

	
	snd_PlaySound(s_song, 1);
	snd_SetSoundVolume(s_song, 90);

	

	// Variables
	unsigned elapsed_time, last_time, new_time;
	unsigned start_time = timeGetTime();
	bool force_update;
	unsigned cmd_move;
	float scale;


	// Init loop variables
	cmd_move = 0;
	last_time = 0;
	force_update = false;
	play_animation = false;
	take_screenshot = false;

	scale = ((float)rand()) / ((float)RAND_MAX)*2.0f + 1.0f;

	int x_flower[50], z_flower[50], x_grass[50], z_grass[50];
	for (int i = 0; i < 50; i++) {
		x_flower[i] = (rand() % 200) - 100;
		z_flower[i] = (rand() % 200) - 100;
	}
	for (int j = 0; j < 50; j++) {
		x_grass[j] = (rand() % 200) - 100;
		z_grass[j] = (rand() % 200) - 100;
	}

	// Game loop
	for (quit = FALSE; NOT quit;) {

		angle += 0.5;
		if (angle >= 360)
			angle = 0;
		gx3d_GetRotateYMatrix(&m, angle);
		gx3d_MultiplyVectorMatrix(&light_position, &m, &(light_data.point.src));
		gx3d_UpdateLight(point_light1, &light_data);

		gx3dVector sound1_position = { 50, 10, 0 };
		gx3dVector Xsound1_position;

		// build a matrix 
		gx3d_MultiplyVectorMatrix(&sound1_position, &m, &Xsound1_position);




		/*____________________________________________________________________
		|
		| Update clock
		|___________________________________________________________________*/

		// Get the current time (# milliseconds since the program started)
		new_time = timeGetTime();
		// Compute the elapsed time (in milliseconds) since the last time through this loop
		if (last_time == 0)
			elapsed_time = 0;
		else
			elapsed_time = new_time - last_time;
		last_time = new_time;

		/*____________________________________________________________________
		|
		| Process user input
		|___________________________________________________________________*/

		// Any event ready?
		if (evGetEvent(&event)) {
			// key press?
			if (event.type == evTYPE_RAW_KEY_PRESS)	{
				// If ESC pressed, exit the program
				if (event.keycode == evKY_ESC)
					quit = TRUE;
				else if (event.keycode == 'w') 
					cmd_move |= POSITION_MOVE_FORWARD;				
				else if (event.keycode == 's') 
					cmd_move |= POSITION_MOVE_BACK;
				else if (event.keycode == 'a') 
					cmd_move |= POSITION_MOVE_LEFT;
				else if (event.keycode == 'd') 
					cmd_move |= POSITION_MOVE_RIGHT;

				else if (event.keycode == evKY_UP_ARROW) {
					facing = 180;
					zmove -= 2;	
					play_animation = true;
					ani_time = -1;
					snd_PlaySound(s_footstep, 0);
				}
				else if (event.keycode == evKY_DOWN_ARROW) {
					facing = 0;
					zmove += 2;	
					play_animation = true;
					ani_time = -1;
					snd_PlaySound(s_footstep, 0);
				}
				else if (event.keycode == evKY_LEFT_ARROW) {
					facing = 270;					
					xmove -= 2;	
					play_animation = true;
					ani_time = -1;
					snd_PlaySound(s_footstep, 0);
				}
				else if (event.keycode == evKY_RIGHT_ARROW) {
					facing = 90;
					xmove += 2;	
					play_animation = true;
					ani_time = -1;
					snd_PlaySound(s_footstep, 0);
				}
				else if (event.keycode == evKY_F1)
					take_screenshot = true;
					
			}
			// key release?
			else if (event.type == evTYPE_RAW_KEY_RELEASE) {
				if (event.keycode == 'w')
					cmd_move &= ~(POSITION_MOVE_FORWARD);
				else if (event.keycode == 's')
					cmd_move &= ~(POSITION_MOVE_BACK);
				else if (event.keycode == 'a')
					cmd_move &= ~(POSITION_MOVE_LEFT);
				else if (event.keycode == 'd')
					cmd_move &= ~(POSITION_MOVE_RIGHT);
			}

			//Footstep sound
			if (cmd_move != 0) {
			    if (! snd_IsPlaying(s_footstep))
			        snd_PlaySound (s_footstep, 1);
			}
			else  {
			    snd_StopSound (s_footstep);			 
			}

		}
		// Check for camera movement (via mouse)
		msGetMouseMovement(&move_x, &move_y);

		/*____________________________________________________________________
		|
		| Update camera view
		|___________________________________________________________________*/

		bool position_changed, camera_changed;
		Position_Update(elapsed_time, cmd_move, -move_y, move_x, force_update,
			&position_changed, &camera_changed, &position, &heading);
		snd_SetListenerPosition(position.x, position.y, position.z, snd_3D_APPLY_NOW);
		snd_SetListenerOrientation(heading.x, heading.y, heading.z, 0, 1, 0, snd_3D_APPLY_NOW);



		/*____________________________________________________________________
		|
		| Draw 3D graphics
		|___________________________________________________________________*/

		// Render the screen
		gx3d_ClearViewport(gx3d_CLEAR_SURFACE | gx3d_CLEAR_ZBUFFER, color, gx3d_MAX_ZBUFFER_VALUE, 0);
		// Start rendering in 3D
		if (gx3d_BeginRender()) {
			// Set the default light
			gx3d_SetAmbientLight(color3d_dim);
			// Set the default material
			gx3d_SetMaterial(&material_default);

			//Draw start screen
			if (timeGetTime() - start_time < 5000) { //10s
				gx3d_SetAmbientLight(color3d_white);
				gx3d_DisableZBuffer();
				gx3d_GetIdentityMatrix(&s);
				gx3d_GetScaleMatrix(&s, 5, 6.5, 3);
				gx3d_SetObjectMatrix(obj_start, &s);
				gx3d_SetTexture(0, tex_start);
				gx3d_DrawObject(obj_start, 0);
				gx3d_EnableZBuffer();
			}
			else {

				//Draw ground
				gx3d_SetAmbientLight(color3d_white);
				gx3d_GetTranslateMatrix(&g, 0, 0, 0);
				gx3d_SetObjectMatrix(obj_ground, &g);
				gx3d_SetTexture(0, tex_ground);
				gx3d_DrawObject(obj_ground, 0); 


				// Enable alpha blending
				gx3d_EnableAlphaBlending();
				gx3d_EnableAlphaTesting(128);


				//Draw character
				characterSphere = obj_character->bound_sphere;
				characterSphere.center.x = characterSphere.center.x*2.5 + xmove;
				characterSphere.center.y = characterSphere.center.y*2.5 + ymove;
				characterSphere.center.z = characterSphere.center.z*2.5 + zmove;
				characterSphere.radius *= 2.5;
				gx3d_GetScaleMatrix(&m1, 2.5, 2.5, 2.5);
				gx3d_GetRotateYMatrix(&m2, facing);
				gx3d_GetTranslateMatrix(&m3, xmove, ymove, zmove);				
				gx3d_MultiplyMatrix(&m1, &m2, &m);
				gx3d_MultiplyMatrix(&m, &m3, &m);
				gx3d_SetObjectMatrix(obj_character, &m);

				//Animation
				// Play the animation, with looping
				if (play_animation) {
					// If this is the start of the animation (anim_time == -1) then set the local timer for the animation to 0
					if (ani_time == -1)
						ani_time = 0;
					// Add the elapsed frame time to the local timer for the animation
					else
						ani_time += elapsed_time;
					// Update the animation based on the local timer          
					gx3d_Motion_Update(motion1, ani_time / 1000.0f, false);
					// Update the animation blend tree
					gx3d_BlendTree_Update(btree1);
				}
				// Just draw neutral pose (first keyframe in idle animation)
				else {
					gx3d_Motion_Update(motion1, 0, false); // keyframe 0 is a neutral pose
					gx3d_BlendTree_Update(btree1);
				}

				gx3d_SetTexture(0, tex_character);
				gx3d_DrawObject(obj_character, 0);

				gx3d_EnableLight(point_light1);

				// Draw a tree
				gx3d_GetScaleMatrix(&m1, 0.5, 0.5, 0.5);
				gx3d_GetTranslateMatrix(&m2, -50, 0, 5);
				gx3d_MultiplyMatrix(&m1, &m2, &m);
				gx3d_SetObjectMatrix(obj_tree, &m);
				gx3d_Object_UpdateTransforms(obj_tree);
				// Draw 2 layer object, by layer
				gx3dObjectLayer *layer;
				layer = gx3d_GetObjectLayer(obj_tree, "trunk");
				gx3d_SetTexture(0, tex_bark);
				gx3d_DrawObjectLayer(layer, 0);
				layer = gx3d_GetObjectLayer(obj_tree, "leaves");
				gx3d_SetTexture(0, tex_tree);
				gx3d_DrawObjectLayer(layer, 0);

				// Draw a smaller tree
				gx3d_GetScaleMatrix(&m1, 0.5, 0.35, 0.5);
				gx3d_GetTranslateMatrix(&m2, 40, 0, -20);
				gx3d_MultiplyMatrix(&m1, &m2, &m);
				gx3d_SetObjectMatrix(obj_tree2, &m);
				gx3d_Object_UpdateTransforms(obj_tree2);
				// Draw 2 layer object, by layer
				gx3dObjectLayer *layer1;
				layer1 = gx3d_GetObjectLayer(obj_tree2, "trunk");
				gx3d_SetTexture(0, tex_bark);
				gx3d_DrawObjectLayer(layer1, 0);
				layer1 = gx3d_GetObjectLayer(obj_tree2, "leaves");
				gx3d_SetTexture(0, tex_tree);
				gx3d_DrawObjectLayer(layer1, 0);

				//Draw tall tree
				gx3d_GetScaleMatrix(&m1, 1, 0.75, 1);
				gx3d_GetTranslateMatrix(&m2, -60, 0, -5);
				gx3d_MultiplyMatrix(&m1, &m2, &m);
				gx3d_SetObjectMatrix(obj_talltree, &m);
				gx3d_SetTexture(0, tex_talltree);
				gx3d_DrawObject(obj_talltree, 0);

				//Draw tall tree
				gx3d_GetScaleMatrix(&m1, 1, 0.65, 1);
				gx3d_GetTranslateMatrix(&m2, 60, 0, 0);
				gx3d_MultiplyMatrix(&m1, &m2, &m);
				gx3d_SetObjectMatrix(obj_talltree, &m);
				gx3d_SetTexture(0, tex_talltree);
				gx3d_DrawObject(obj_talltree, 0);
				
				gx3d_DisableLight(point_light1);

				//Draw grass and flowers
				for (int i = 0; i < 50; i++) {
					gx3d_GetScaleMatrix(&m1, scale, scale, scale);
					gx3d_GetTranslateMatrix(&m2, x_flower[i], 0, z_flower[i]);
					gx3d_MultiplyMatrix(&m1, &m2, &m);
					gx3d_SetObjectMatrix(obj_flower, &m);
					gx3d_SetTexture(0, tex_flower);
					gx3d_DrawObject(obj_flower, 0);
				}

				for (int j = 0; j < 50; j++) {
					gx3d_GetTranslateMatrix(&m, x_grass[j], 0, z_grass[j]);
					gx3d_SetObjectMatrix(obj_grass, &m);
					gx3d_SetTexture(0, tex_grass);
					gx3d_DrawObject(obj_grass, 0);
				}

				//Draw mountain
				gx3d_GetScaleMatrix(&m1, 6, 6, 6);
				gx3d_GetRotateYMatrix(&m2, 180);
				gx3d_GetTranslateMatrix(&m3, 0, 0, 300);
				gx3d_MultiplyMatrix(&m1, &m2, &mo);
				gx3d_MultiplyMatrix(&mo, &m3, &mo);
				gx3d_SetObjectMatrix(obj_mountain, &mo);
				gx3d_SetTexture(0, tex_mountain);
				gx3d_DrawObject(obj_mountain, 0);

				gx3d_GetScaleMatrix(&m1, 6, 6, 6);
				gx3d_GetRotateYMatrix(&m2, 180);
				gx3d_GetTranslateMatrix(&m3, 120, 0, 300);
				gx3d_MultiplyMatrix(&m1, &m2, &mo);
				gx3d_MultiplyMatrix(&mo, &m3, &mo);
				gx3d_SetObjectMatrix(obj_mountain, &mo);
				gx3d_SetTexture(0, tex_mountain);
				gx3d_DrawObject(obj_mountain, 0);

				gx3d_GetScaleMatrix(&m1, 6, 6, 6);
				gx3d_GetRotateYMatrix(&m2, 180);
				gx3d_GetTranslateMatrix(&m3, -120, 0, 300);
				gx3d_MultiplyMatrix(&m1, &m2, &mo);
				gx3d_MultiplyMatrix(&mo, &m3, &mo);
				gx3d_SetObjectMatrix(obj_mountain, &mo);
				gx3d_SetTexture(0, tex_mountain);
				gx3d_DrawObject(obj_mountain, 0);

				gx3d_GetScaleMatrix(&m1, 6, 6, 6);
				gx3d_GetRotateYMatrix(&m2, 180);
				gx3d_GetTranslateMatrix(&m3, -240, 0, 300);
				gx3d_MultiplyMatrix(&m1, &m2, &mo);
				gx3d_MultiplyMatrix(&mo, &m3, &mo);
				gx3d_SetObjectMatrix(obj_mountain, &mo);
				gx3d_SetTexture(0, tex_mountain);
				gx3d_DrawObject(obj_mountain, 0);

				gx3d_DisableAlphaTesting();

				// Draw skydome
				gx3d_SetAmbientLight(color3d_white);
				gx3d_DisableLight(dir_light);
				gx3d_GetScaleMatrix(&m1, 500, 500, 500);
				gx3d_SetTexture(0, tex_skydome);
				gx3d_SetObjectMatrix(obj_skydome, &m1);
				gx3d_DrawObject(obj_skydome, 0);

				// Draw clouds
				static float offset = 0;
				offset += 0.001;
				if (offset > 1.0)
					offset = 0;

				static float angle = 0;
				angle += 1;
				if (angle == 360)
					angle = 0;

				// Turn on fog
				//gx3d_EnableFog();
				//gx3d_SetFogColor(0, 0, 0);
				//			gx3d_SetLinearPixelFog (450, 550);
				//gx3d_SetExp2PixelFog(0.005);  // 0-1

				gx3d_SetAmbientLight(color3d_white);
				gx3d_EnableTextureMatrix(0);
				gx3d_GetTranslateTextureMatrix(&m, -0.5, -0.5);
				gx3d_GetRotateTextureMatrix(&m2, angle);
				gx3d_GetTranslateTextureMatrix(&m3, 0.5, 0.5);
				gx3d_MultiplyMatrix(&m, &m2, &m);
				gx3d_MultiplyMatrix(&m, &m3, &m);
				gx3d_SetTextureMatrix(0, &m);

				gx3d_GetScaleMatrix(&m1, 500, 500, 500);
				gx3d_SetObjectMatrix(obj_clouddome, &m1);
				gx3d_SetTexture(0, tex_clouddome);
				gx3d_DrawObject(obj_clouddome, 0);

				gx3d_DisableTextureMatrix(0);				
				gx3d_DisableLight(dir_light);

				// Turn off fog
				//gx3d_DisableFog();

				//Draw power
				for (int i = 0; i < NUM_POWER; i++) {
					powerPosition[i].y -= powerSpeed[i];
					if (powerPosition[i].y <= 0) {
						powerPosition[i].y = height[i];
						num_die++;
						if (num_die > 5) {
							game_over = true;
						}
					}
				}

				static gx3dVector billboard_normal = { 0, 0, 1 };
				gxRelation powerRelation;
				for (int i = 0; i < NUM_POWER; i++) {
					if (powerDraw[i]) {
						powerSphere[i] = obj_power->bound_sphere;
						powerSphere[i].center.x += powerPosition[i].x;
						powerSphere[i].center.y += powerPosition[i].y;
						powerSphere[i].center.z += powerPosition[i].z;
						powerSphere[i].radius *= 1.5;
						powerRelation = gx3d_Relation_Sphere_Sphere(&characterSphere, &powerSphere[i], false);
						if (powerRelation != gxRELATION_OUTSIDE) {
							powerDraw[i] = false;
							snd_PlaySound(s_yeah, 0);
							//create a hit
							hitPosition[hitIndex] = powerSphere[i].center;
							hitTimer[hitIndex] = 500;// +elapsed_time;
							hitIndex = (hitIndex + 1) % MAX_HIT;	
						}
						if (powerDraw[i]) {
							gx3d_GetScaleMatrix(&m1, 1.5, 1.5, 1.5);
							gx3d_GetBillboardRotateYMatrix(&m2, &billboard_normal, &heading);
							gx3d_GetTranslateMatrix(&m3, powerPosition[i].x, powerPosition[i].y, powerPosition[i].z);
							gx3d_MultiplyMatrix(&m1, &m2, &m);
							gx3d_MultiplyMatrix(&m, &m3, &m);
							gx3d_SetObjectMatrix(obj_power, &m);
							gx3d_SetTexture(0, tex_purple);
							gx3d_DrawObject(obj_power, 0);

							//Draw partical system
							gx3d_SetParticleSystemMatrix(psys_power, &m);
							gx3d_UpdateParticleSystem(psys_power, elapsed_time);
							gx3d_DrawParticleSystem(psys_power, &heading, false);
						}
					}
				}

				//Draw explosion
				for (int i = 0; i < NUM_EXPLOSION; i++) {
					exPosition[i].y -= exSpeed[i];
					if (exPosition[i].y <= 0) {
						exPosition[i].y = exheight[i];
					}
				}

				gxRelation exRelation;
				for (int i = 0; i < NUM_EXPLOSION; i++) {
					if (exDraw[i]) {
						exSphere[i] = obj_explosion->bound_sphere;
						exSphere[i].center.x += exPosition[i].x;
						exSphere[i].center.y += exPosition[i].y;
						exSphere[i].center.z += exPosition[i].z;
						exSphere[i].radius *= 1.5;
						exRelation = gx3d_Relation_Sphere_Sphere(&characterSphere, &exSphere[i], false);
						if (exRelation != gxRELATION_OUTSIDE) {
							exDraw[i] = false;
							snd_PlaySound(s_boom, 0);
							//create a hit
							exhitPosition[exhitIndex] = exSphere[i].center;
							exhitTimer[exhitIndex] = 500;// +elapsed_time;
							exhitIndex = (exhitIndex + 1) % exMAX_HIT;
			
							num_die++;
							if (num_die > 5) {
								game_over = true;
							}
						}
						if (exDraw[i]) {
							gx3d_GetScaleMatrix(&m1, 1.5, 1.5, 1.5);
							gx3d_GetBillboardRotateYMatrix(&m2, &billboard_normal, &heading);
							gx3d_GetTranslateMatrix(&m3, exPosition[i].x, exPosition[i].y, exPosition[i].z);
							gx3d_MultiplyMatrix(&m1, &m2, &m);
							gx3d_MultiplyMatrix(&m, &m3, &m);
							gx3d_SetObjectMatrix(obj_explosion, &m);
							gx3d_SetTexture(0, tex_explosion);
							gx3d_DrawObject(obj_explosion, 0);

							//Draw partical system
							gx3d_SetParticleSystemMatrix(psys_fire, &m);
							gx3d_UpdateParticleSystem(psys_fire, elapsed_time);
							gx3d_DrawParticleSystem(psys_fire, &heading, false);
						}
					}
				}

				

				/*____________________________________________________________________
				|
				| Process hit makers
				|___________________________________________________________________*/

				const float HIT_SCALE = 3;
				//Update hit makers timer
				for (int i = 0; i < MAX_HIT; i++) {
					if (hitTimer[i] > 0)
						hitTimer[i] -= elapsed_time;
				}

				//Draw any hit makers
				gx3d_EnableAlphaBlending();
				gx3d_EnableAlphaTesting(128);
				for (int i = 0; i < MAX_HIT; i++) {
					if (hitTimer[i] > 0) {
						gx3d_GetScaleMatrix(&m1, HIT_SCALE, HIT_SCALE, HIT_SCALE);
						gx3d_GetBillboardRotateYMatrix(&m2, &billboard_normal, &heading);
						float y = hitPosition[i].y + (1 - (hitTimer[i] / 1000.0f))*(2 * HIT_SCALE);
						gx3d_GetTranslateMatrix(&m3, hitPosition[i].x, y + 12, hitPosition[i].z);
						gx3d_MultiplyMatrix(&m1, &m2, &m);
						gx3d_MultiplyMatrix(&m, &m3, &m);
						gx3d_SetObjectMatrix(obj_hit, &m);
						gx3d_SetTexture(0, tex_hit);
						gx3d_DrawObject(obj_hit, 0);
					}
				}

				//exploision
				const float exHIT_SCALE = 3;
				//Update hit makers timer
				for (int i = 0; i < exMAX_HIT; i++) {
					if (exhitTimer[i] > 0)
						exhitTimer[i] -= elapsed_time;
				}

				//Draw any hit makers
				for (int i = 0; i < exMAX_HIT; i++) {
					if (exhitTimer[i] > 0) {
						gx3d_GetScaleMatrix(&m1, exHIT_SCALE, exHIT_SCALE, exHIT_SCALE);
						gx3d_GetBillboardRotateYMatrix(&m2, &billboard_normal, &heading);
						float y = exhitPosition[i].y + (1 - (exhitTimer[i] / 1000.0f))*(2 * exHIT_SCALE);
						gx3d_GetTranslateMatrix(&m3, exhitPosition[i].x, y + 12, exhitPosition[i].z);
						gx3d_MultiplyMatrix(&m1, &m2, &m);
						gx3d_MultiplyMatrix(&m, &m3, &m);
						gx3d_SetObjectMatrix(obj_hit, &m);
						gx3d_SetTexture(0, tex_boom);
						gx3d_DrawObject(obj_hit, 0);
					}
				}

				gx3d_DisableAlphaBlending();
				gx3d_DisableAlphaTesting();

				/*____________________________________________________________________
				|
				| Draw 2D graphics on top of 3D
				|___________________________________________________________________*/

				//Save current view matrix
				gx3dMatrix view_save;
				gx3d_GetViewMatrix(&view_save);

				//Set new view matrix
				gx3dVector tfrom = { 0, 0, -1 }, tto = { 0, 0, 0 }, twup = { 0, 1, 0 };
				gx3d_CameraSetPosition(&tfrom, &tto, &twup, gx3d_CAMERA_ORIENTATION_LOOKTO_FIXED);
				gx3d_CameraSetViewMatrix();

				//Draw 2D icons at top of screen
				if (num_die) {
					gx3d_DisableZBuffer();
					gx3d_EnableAlphaBlending();
					for (int i = 0; i < num_die; i++) {
						gx3d_GetScaleMatrix(&m1, 0.015f, 0.015f, 0.015f);
						gx3d_GetRotateYMatrix(&m2, 180);
						gx3d_GetTranslateMatrix(&m3, -0.5 + (0.06*i), 0.25, 0);
						gx3d_MultiplyMatrix(&m1, &m2, &m);
						gx3d_MultiplyMatrix(&m, &m3, &m);
						gx3d_SetObjectMatrix(obj_die, &m);
						gx3d_SetTexture(0, tex_die);
						gx3d_DrawObject(obj_die, 0);
					}
					gx3d_DisableAlphaBlending();
					gx3d_EnableZBuffer();
				}

				//Restore view matrix
				gx3d_SetViewMatrix(&view_save);

				

				//Game over
				if (game_over) { 
					snd_StopSound(s_song);
					snd_StopSound(s_yeah);
					snd_StopSound(s_boom);
					snd_PlaySound(s_over, 0);										

					for (int i = 0; i < NUM_POWER; i++)
						powerDraw[i] = false;
					for (int i = 0; i < NUM_EXPLOSION; i++)
						exDraw[i] = false;

					gx3d_SetAmbientLight(color3d_white);
					gx3d_DisableZBuffer();
					gx3d_GetIdentityMatrix(&s);
					gx3d_GetScaleMatrix(&s, 5, 6.5, 3);
					gx3d_SetObjectMatrix(obj_start, &s);
					gx3d_SetTexture(0, tex_over);
					gx3d_DrawObject(obj_start, 0);
					gx3d_EnableZBuffer();
				}

			}//end else

			// Stop rendering
			gx3d_EndRender();

			// Take screenshot
			if (take_screenshot) {
				char str[32];
				char buff[50];
				static int screenshot_count = 0;
				strcpy(str, SCREENSHOT_FILENAME);
				strcat(str, itoa(screenshot_count++, buff, 10));
				strcat(str, ".bmp");
				gxWriteBMPFile(str);
				take_screenshot = false;
			}

			// Page flip (so user can see it)
			gxFlipVisualActivePages(FALSE);

		}
	}

	/*____________________________________________________________________
	|
	| Free stuff and exit
	|___________________________________________________________________*/

	gx3d_FreeObject(obj_tree);
	gx3d_FreeObject(obj_tree2);
	gx3d_FreeObject(obj_skydome);
	gx3d_FreeObject(obj_ground);
	gx3d_FreeObject(obj_power);
	gx3d_FreeObject(obj_mountain);
	gx3d_FreeObject(obj_talltree);
	gx3d_FreeObject(obj_grass);
	gx3d_FreeObject(obj_character);
	gx3d_FreeObject(obj_flower);
	gx3d_FreeParticleSystem(psys_fire);
	gx3d_FreeParticleSystem(psys_power);
	gx3d_Motion_Free(motion1);
	gx3d_BlendNode_Free(bnode1);
	gx3d_BlendTree_Free(btree1);

	snd_StopSound(s_song);
	snd_Free();
}

/*____________________________________________________________________
|
| Function: Init_Render_State
|
| Input: Called from Program_Run()
| Output: Initializes general 3D render state.
|___________________________________________________________________*/

static void Init_Render_State()
{
	// Enable zbuffering
	gx3d_EnableZBuffer();

	// Enable lighting
	gx3d_EnableLighting();

	// Set the default alpha blend factor
	gx3d_SetAlphaBlendFactor(gx3d_ALPHABLENDFACTOR_SRCALPHA, gx3d_ALPHABLENDFACTOR_INVSRCALPHA);

	// Init texture addressing mode - wrap in both u and v dimensions
	gx3d_SetTextureAddressingMode(0, gx3d_TEXTURE_DIMENSION_U | gx3d_TEXTURE_DIMENSION_V, gx3d_TEXTURE_ADDRESSMODE_WRAP);
	gx3d_SetTextureAddressingMode(1, gx3d_TEXTURE_DIMENSION_U | gx3d_TEXTURE_DIMENSION_V, gx3d_TEXTURE_ADDRESSMODE_WRAP);
	// Texture stage 0 default blend operator and arguments
	gx3d_SetTextureColorOp(0, gx3d_TEXTURE_COLOROP_MODULATE, gx3d_TEXTURE_ARG_TEXTURE, gx3d_TEXTURE_ARG_CURRENT);
	gx3d_SetTextureAlphaOp(0, gx3d_TEXTURE_ALPHAOP_SELECTARG1, gx3d_TEXTURE_ARG_TEXTURE, 0);
	// Texture stage 1 is off by default
	gx3d_SetTextureColorOp(1, gx3d_TEXTURE_COLOROP_DISABLE, 0, 0);
	gx3d_SetTextureAlphaOp(1, gx3d_TEXTURE_ALPHAOP_DISABLE, 0, 0);

	// Set default texture coordinates
	gx3d_SetTextureCoordinates(0, gx3d_TEXCOORD_SET0);
	gx3d_SetTextureCoordinates(1, gx3d_TEXCOORD_SET1);

	// Enable trilinear texture filtering
	gx3d_SetTextureFiltering(0, gx3d_TEXTURE_FILTERTYPE_TRILINEAR, 0);
	gx3d_SetTextureFiltering(1, gx3d_TEXTURE_FILTERTYPE_TRILINEAR, 0);
}

/*____________________________________________________________________
|
| Function: Load_Motion
|
| Input: Called from Program_Run()
| Output: Loads a gx3dMotion and prints info to DEBUG.TXT.
|___________________________________________________________________*/

static gx3dMotion *Load_Motion(gx3dMotionSkeleton *mskeleton, char *filename, int fps, gx3dMotionMetadataRequest *metadata_requested, int num_metadata_requested, bool load_all_metadata)
{
	int i;
	char str[250];
	gx3dMotion *motion = 0;

	motion = gx3d_Motion_Read_LWS_File(mskeleton, filename, fps, metadata_requested, num_metadata_requested, load_all_metadata);
	if (motion) {
		sprintf(str, "ANIMATION LOADED: %s", filename);
		DEBUG_WRITE(str);
		sprintf(str, "  Fps: %d", motion->keys_per_second);
		DEBUG_WRITE(str);
		sprintf(str, "  # keys: %d", motion->max_nkeys);
		DEBUG_WRITE(str);
		//    sprintf (str, "  Duration: %f", 1.0 / motion->keys_per_second * (motion->max_nkeys-1));
		//    DEBUG_WRITE (str);                              
		sprintf(str, "  Duration: %f", motion->duration / 1000.0f);
		DEBUG_WRITE(str);
		for (i = 0; i<motion->num_metadata; i++) {
			sprintf(str, "  Metadata: %s", motion->metadata[i].name);
			DEBUG_WRITE(str);
		}
		DEBUG_WRITE("");
	}
	return (motion);
}


/*____________________________________________________________________
|
| Function: Program_Free
|
| Input: Called from CMainFrame::OnClose()
| Output: Exits graphics mode.
|___________________________________________________________________*/

void Program_Free()
{
	// Stop event processing 
	evStopEvents();
	// Return to text mode 
	if (Pgm_system_font)
		gxFreeFont(Pgm_system_font);
	gxStopGraphics();
}
