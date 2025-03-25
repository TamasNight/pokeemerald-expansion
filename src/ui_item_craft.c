#include "global.h"
#include "ui_item_craft.h"
#include "strings.h"
#include "bg.h"
#include "data.h"
#include "decompress.h"
#include "event_data.h"
#include "field_weather.h"
#include "gpu_regs.h"
#include "graphics.h"
#include "item.h"
#include "item_menu.h"
#include "item_menu_icons.h"
#include "list_menu.h"
#include "item_icon.h"
#include "item_use.h"
#include "international_string_util.h"
#include "main.h"
#include "malloc.h"
#include "menu.h"
#include "menu_helpers.h"
#include "palette.h"
#include "party_menu.h"
#include "scanline_effect.h"
#include "script.h"
#include "sound.h"
#include "string_util.h"
#include "strings.h"
#include "task.h"
#include "text_window.h"
#include "overworld.h"
#include "event_data.h"
#include "constants/items.h"
#include "constants/field_weather.h"
#include "constants/songs.h"
#include "constants/rgb.h"
#include "trainer_pokemon_sprites.h"
#include "field_effect.h"
#include "pokedex.h"
#include "script_pokemon_util.h"
#include "pokeball.h"
#include "constants/moves.h"
#include "naming_screen.h"
#include "tv.h"


//==========DEFINES==========//
struct MenuItemResources
{
    MainCallback savedCallback;     // determines callback to run when we exit. e.g. where do we want to go after closing the menu
    u8 gfxLoadState;
    u16 itemSpriteIds[3];
    u16 cursorSpriteId;
    bool8 movingSelector;
    u8 cursorPosition;
};



//==========EWRAM==========//
static EWRAM_DATA struct MenuItemResources *sItemCraftDataPtr = NULL;
static EWRAM_DATA u8 *sBg1TilemapBuffer = NULL;
static EWRAM_DATA u8 *sBg2TilemapBuffer = NULL;


void Task_OpenItemCraft(u8 taskId) {
    if (!gPaletteFade.active)
    {
        CleanupOverworldWindowsAndTilemaps();
        ItemCraft_Init(CB2_ReturnToBagMenuPocket);
        DestroyTask(taskId);
    }
}

void ItemCraft_Init(MainCallback callback) 
{
    u16 i = 0;
    if ((sItemCraftDataPtr = AllocZeroed(sizeof(struct MenuItemResources))) == NULL)
    {
        SetMainCallback2(callback);
        return;
    }
    
    // initialize stuff
    sItemCraftDataPtr->gfxLoadState = 0;
    sItemCraftDataPtr->savedCallback = callback;
    sItemCraftDataPtr->cursorSpriteId = 0xFFFF;
    for (i = 0; i < 3; i++)
        sItemCraftDataPtr->itemSpriteIds[i] = 0xFFFF;
    sItemCraftDataPtr->movingSelector = FALSE;
    sItemCraftDataPtr->cursorPosition = 0;

    SetMainCallback2(ItemCraftRunSetup);
}

static void ItemCraftRunSetup(void)
{
    while (1)
    {
        if (ItemCraftDoGfxSetup() == TRUE)
            break;
    }
}

static void ItemCraftMainCB(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    DoScheduledBgTilemapCopiesToVram();
    UpdatePaletteFade();
}

static void ItemCraftVBlankCB(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

static bool8 ItemCradrDoGfxSetup(void) {
    //TODO loop di init
    return FALSE;
}
