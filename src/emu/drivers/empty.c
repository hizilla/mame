/*************************************************************************

    empty.c

    Empty driver.

    Copyright (c) 1996-2007, Nicola Salmoria and the MAME Team.
    Visit http://mamedev.org for licensing and usage restrictions.

**************************************************************************/

#include "driver.h"
#include "uimenu.h"


/*************************************
 *
 *  Machine "start"
 *
 *************************************/

static MACHINE_START( empty )
{
	/* force the UI to show the game select screen */
	printf("empty machine start.\n");
	// ui_menu_force_game_select();
}



/*************************************
 *
 *  Machine drivers
 *
 *************************************/

static MACHINE_DRIVER_START( empty )

	MDRV_MACHINE_START(empty)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)

	MDRV_SCREEN_ADD("main", 0)
	MDRV_SCREEN_FORMAT(BITMAP_FORMAT_RGB32)
	MDRV_SCREEN_SIZE(640,480)
	MDRV_SCREEN_VISIBLE_AREA(0,639, 0,479)
	MDRV_SCREEN_REFRESH_RATE(30)
MACHINE_DRIVER_END



/*************************************
 *
 *  Input ports
 *
 *************************************/

static INPUT_PORTS_START( empty )
INPUT_PORTS_END



/*************************************
 *
 *  ROM definitions
 *
 *************************************/

ROM_START( empty )
	ROM_REGION( 0x10, REGION_USER1, 0 )
ROM_END



/*************************************
 *
 *  Game drivers
 *
 *************************************/

GAME( 2007, empty, 0, empty, empty, 0, ROT0, "MAME", "No Driver Loaded", 0 )
