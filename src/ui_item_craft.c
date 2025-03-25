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

struct Recipe
{
    u16 item1;
    u8 item1Amount;
    u16 item2;
    u8 item2Amount;
    u16 result;
};

#define WINDOW_ITEM_CRAFT   1
#define CHOOSE_ITEM         0
#define CONFIRM_SELECTION   1
#define RECIEVED_ITEM       2
#define TAG_CURSOR          30012
#define FIRST_RECIPE        0
#define SECOND_RECIPE       1
#define CURSOR_MAX_POSITION SECOND_RECIPE

#define try_free(ptr) ({        \
    void ** ptr__ = (void **)&(ptr);   \
    if (*ptr__ != NULL)                \
        Free(*ptr__);                  \
})

enum Colors
{
    FONT_BLACK,
    FONT_WHITE,
    FONT_RED,
    FONT_BLUE,
};
static const u8 sMenuWindowFontColors[][3] = 
{
    [FONT_BLACK]  = {TEXT_COLOR_TRANSPARENT,  TEXT_COLOR_DARK_GRAY,  TEXT_COLOR_LIGHT_GRAY},
    [FONT_WHITE]  = {TEXT_COLOR_TRANSPARENT,  TEXT_COLOR_WHITE,  TEXT_COLOR_DARK_GRAY},
    [FONT_RED]   = {TEXT_COLOR_TRANSPARENT,  TEXT_COLOR_RED,        TEXT_COLOR_LIGHT_GRAY},
    [FONT_BLUE]  = {TEXT_COLOR_TRANSPARENT,  TEXT_COLOR_BLUE,       TEXT_COLOR_LIGHT_GRAY},
};
static const u8 sRecipeList[CURSOR_MAX_POSITION + 1] =
{
    [FIRST_RECIPE]  = {ITEM_STEEL_SHARD, 10, ITEM_NONE, 0, ITEM_BOTTLE_CAP},
    [SECOND_RECIPE] = {ITEM_STEEL_SHARD, 10, ITEM_DRAGON_SHARD, 10, ITEM_GOLD_BOTTLE_CAP},
};


//==========STATIC=DECLARATIONS==========//
static void ItemCraftRunSetup(void);
static bool8 ItemCraftDoGfxSetup(void);
static bool8 ItemCraft_InitBgs(void);
static void ItemCraftFadeAndBail(void);
static bool8 ItemCraftLoadGraphics(void);
static void ItemCraft_InitWindows(void);
static void PrintTextToBottomBox(u8 textId);
static void Task_ItemCraftMain(u8 taskId);
static void Task_ItemCraftWaitFadeIn(u8 taskId);



//==========CONST=DATA==========//
static const struct BgTemplate sItemCraftBgTemplates[] =
{
    {
        .bg = 0,    // windows, etc
        .charBaseIndex = 0,
        .mapBaseIndex = 31,
        .priority = 0
    }, 
    {
        .bg = 1,    // this bg loads the UI tilemap 
        .charBaseIndex = 3,
        .mapBaseIndex = 30,
        .priority = 2
    },
    {
        .bg = 2,    // this bg loads the UI tilemap - semitransparent
        .charBaseIndex = 2,
        .mapBaseIndex = 28,
        .priority = 1
    }
};

static const struct WindowTemplate sItemCraftWindowTemplates[] = 
{
    [WINDOW_ITEM_CRAFT] = 
    {
        .bg = 0,            // which bg to print text on
        .tilemapLeft = 0,   // position from left (per 8 pixels)
        .tilemapTop = 14,   // position from top (per 8 pixels)
        .width = 30,        // width (per 8 pixels)
        .height = 6,        // height (per 8 pixels)
        .paletteNum = 15,   // palette index to use for text
        .baseBlock = 1,     // tile start in VRAM
    },
    DUMMY_WIN_TEMPLATE
};

//
//  Graphics Pointers to Tilemaps, Tilesets, Spritesheets, Palettes
//
static const u32 sBgTiles[]   = INCBIN_U32("graphics/ui_item_craft/bg_tiles.4bpp.lz");
static const u32 sBgTilemap[] = INCBIN_U32("graphics/ui_item_craft/bg_tiles.bin.lz");
static const u16 sBgPalette[] = INCBIN_U16("graphics/ui_item_craft/bg_tiles.gbapal");

static const u32 sTextBgTiles[]   = INCBIN_U32("graphics/ui_item_craft/text_bg_tiles.4bpp.lz");
static const u32 sTextBgTilemap[] = INCBIN_U32("graphics/ui_item_craft/text_bg_tiles.bin.lz");
static const u16 sTextBgPalette[] = INCBIN_U16("graphics/ui_item_craft/text_bg_tiles.gbapal");

static const u32 sCursor_Gfx[] = INCBIN_U32("graphics/item_craft/cursor.4bpp.lz");
static const u16 sCursor_Pal[] = INCBIN_U16("graphics/item_craft/cursor.gbapal");


static const struct OamData sOamData_Cursor =
{
    .size = SPRITE_SIZE(64x64),
    .shape = SPRITE_SHAPE(64x64),
    .priority = 1,
};

static const struct CompressedSpriteSheet sSpriteSheet_Cursor =
{
    .data = sCursor_Gfx,
    .size = 64*64*1/2,
    .tag = TAG_CURSOR,
};

static const struct SpritePalette sSpritePal_Cursor =
{
    .data = sCursor_Pal,
    .tag = TAG_CURSOR
};

static const union AnimCmd sSpriteAnim_Cursor[] =
{
    ANIMCMD_FRAME(0, 32),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd *const sSpriteAnimTable_Cursor[] =
{
    sSpriteAnim_Cursor,
};

static const struct SpriteTemplate sSpriteTemplate_Cursor =
{
    .tileTag = TAG_CURSOR,
    .paletteTag = TAG_CURSOR,
    .oam = &sOamData_Cursor,
    .anims = sSpriteAnimTable_Cursor,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy
};



//==========EWRAM==========//
static EWRAM_DATA struct MenuItemResources *sItemCraftDataPtr = NULL;
static EWRAM_DATA u8 *sBg1TilemapBuffer = NULL;     // bg1 sfondo 
static EWRAM_DATA u8 *sBg2TilemapBuffer = NULL;     // bg2 layer di UI semitransparente



//==========FUNCTIONS==========//
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

static bool8 ItemCraftDoGfxSetup(void) {
    switch (gMain.state)
    {
    case 0:
        DmaClearLarge16(3, (void *)VRAM, VRAM_SIZE, 0x1000)
        SetVBlankHBlankCallbacksToNull();
        ClearScheduledBgCopiesToVram();
        ResetVramOamAndBgCntRegs();
        gMain.state++;
        break;
    case 1:
        ScanlineEffect_Stop();
        FreeAllSpritePalettes();
        ResetPaletteFade();
        ResetSpriteData();
        ResetTasks();
        gMain.state++;
        break;
    case 2:
        if (ItemCraft_InitBgs())
        {
            sItemCraftDataPtr->gfxLoadState = 0;
            gMain.state++;
        }
        else
        {
            ItemCraftFadeAndBail();
            return TRUE;
        }
        break;
    case 3:
        if (ItemCraftLoadGraphics() == TRUE)
            gMain.state++;
        break;
        case 4:
        LoadMessageBoxAndBorderGfx();
        ItemCraft_InitWindows();
        gMain.state++;
        break;
    case 5:
        CreateCursorSprite();
        PrintTextToBottomBox(CHOOSE_ITEM);
        CreateTask(Task_ItemCraftWaitFadeIn, 0);
        BlendPalettes(0xFFFFFFFF, 16, RGB_BLACK);
        gMain.state++;
        break;
    case 6:
        BeginNormalPaletteFade(0xFFFFFFFF, 0, 16, 0, RGB_BLACK);
        gMain.state++;
        break;
    default:
        SetVBlankCallback(ItemCraftVBlankCB);
        SetMainCallback2(ItemCraftMainCB);
        return TRUE;
    }
    return FALSE;
}

static bool8 ItemCraft_InitBgs(void) {
    ResetAllBgsCoordinates();
    sBg1TilemapBuffer = Alloc(0x800);
    if (sBg1TilemapBuffer == NULL)
        return FALSE;
    memset(sBg1TilemapBuffer, 0, 0x800);

    sBg2TilemapBuffer = Alloc(0x800);
    if (sBg2TilemapBuffer == NULL)
        return FALSE;
    memset(sBg2TilemapBuffer, 0, 0x800);
    
    
    ResetBgsAndClearDma3BusyFlags(0);
    InitBgsFromTemplates(0, sItemCraftBgTemplates, NELEMS(sItemCraftBgTemplates));
    SetBgTilemapBuffer(1, sBg1TilemapBuffer);
    SetBgTilemapBuffer(2, sBg2TilemapBuffer);
    ScheduleBgCopyTilemapToVram(1);
    ScheduleBgCopyTilemapToVram(2);
    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP);
    SetGpuReg(REG_OFFSET_BLDCNT, BLDCNT_TGT2_ALL | BLDCNT_EFFECT_BLEND | BLDCNT_TGT1_BG2);
    SetGpuReg(REG_OFFSET_BLDALPHA, BLDALPHA_BLEND(1, 8));
    ShowBg(0);
    ShowBg(1);
    ShowBg(2);
    return TRUE;
}

static void ItemCraftFadeAndBail(void) {
    BeginNormalPaletteFade(0xFFFFFFFF, 0, 0, 16, RGB_BLACK);
    CreateTask(Task_ItemCraftWaitFadeAndBail, 0);
    SetVBlankCallback(ItemCraftVBlankCB);
    SetMainCallback2(ItemCraftMainCB);
}

static void Task_ItemCraftWaitFadeAndBail(u8 taskId) {
    if (!gPaletteFade.active)
    {
        SetMainCallback2(sItemCraftDataPtr->savedCallback);
        ItemCraftFreeResources();
        DestroyTask(taskId);
    }
}

static void ItemCraftFreeResource(void) {
    try_free(sItemCraftDataPtr);
    try_free(sBg1TilemapBuffer);
    try_free(sBg2TilemapBuffer);
    // TODO pulire le sprite
    FreeAllWindowBuffers();
}

static bool8 ItemCraftLoadGraphics(void) {
    switch (sItemCraftDataPtr->gfxLoadState)
    {
    case 0:
        ResetTempTileDataBuffers();
        DecompressAndCopyTileDataToVram(1, sBgTiles, 0, 0, 0);
        DecompressAndCopyTileDataToVram(2, sTextBgTiles, 0, 0, 0);
        sItemCraftDataPtr->gfxLoadState++;
        break;
    case 1:
        if (FreeTempTileDataBuffersIfPossible() != TRUE)
        {
            LZDecompressWram(sBgTilemap, sBg1TilemapBuffer);
            LZDecompressWram(sTextBgTilemap, sBg2TilemapBuffer);
            sItemCraftDataPtr->gfxLoadState++;
        }
        break;
    case 2:
        LoadCompressedSpriteSheet(&sSpriteSheet_Cursor);
        LoadSpritePalette(&sSpritePal_Cursor);
        LoadPalette(sBgPalette, 32, 32);
        LoadPalette(sTextBgPalette, 16, 16);
        sItemCraftDataPtr->gfxLoadState++;
        break;
    default:
        sItemCraftDataPtr->gfxLoadState = 0;
        return TRUE;
    }
    return FALSE;
}

//
//  Text Printing Function
//
static const u8 sText_ChooseItem[] = _("Select a recipe to craft!");
static const u8 sText_AreYouSure[] = _("Are you sure?    {A_BUTTON} Yes  {B_BUTTON} No");
static const u8 sText_RecievedItem[] = _("You have crafted a {BUFFER_1}!");
static void PrintTextToBottomBox(u8 textId)
{
    u8 speciesNameArray[16];
    const u8 *mainBarAlternatingText;
    const u8 * speciesCategoryText;

    u8 x = 1 + 4;
    u8 y = 1 + 18;  

    FillWindowPixelBuffer(WINDOW_ITEM_CRAFT, PIXEL_FILL(TEXT_COLOR_TRANSPARENT));

    switch(textId)
    {
        case CHOOSE_ITEM:
            mainBarAlternatingText = sText_ChooseItem;
            break;
        case 1:
            mainBarAlternatingText = sText_AreYouSure;
            break;
        case 2:
            mainBarAlternatingText = sText_RecievedItem;
            break;
        default:
            mainBarAlternatingText = sText_ChooseItem;
            break;
    } 
    AddTextPrinterParameterized4(WINDOW_ITEM_CRAFT, FONT_NORMAL, x, y, 0, 0, sMenuWindowFontColors[FONT_WHITE], 0xFF, mainBarAlternatingText);
    
    PutWindowTilemap(WINDOW_ITEM_CRAFT);
    CopyWindowToVram(WINDOW_ITEM_CRAFT, 3);
}

static void Task_ItemCraftWaitFadeIn(u8 taskId)
{
    if (!gPaletteFade.active)
        gTasks[taskId].func = Task_ItemCraftMain;
}

static void ChangePositionUpdateSpriteAnims(u16 oldPosition, u8 taskId) // turn off Ball Shaking on old ball and start it on new ball, reload pokemon and text
{
    //StartSpriteAnim(&gSprites[sBirchCaseDataPtr->pokeballSpriteIds[oldPosition]], 0);
    //StartSpriteAnim(&gSprites[sBirchCaseDataPtr->pokeballSpriteIds[sBirchCaseDataPtr->handPosition]], 1);
    //ReloadNewPokemon(taskId);
    PrintTextToBottomBox(CHOOSE_ITEM);
}

static void BirchCase_GiveItem() {
    // TODO giveitem getting the item from the sRecipeList[sItemCraftDataPtr->cursorPosition]
}

static void Task_ItemCraftRecievedItem(u8 taskId) {
    // TODO return to maintasks
}

static void Task_ItemCraftConfirmSelection(u8 taskId)
{
    if(JOY_NEW(A_BUTTON))
    {
        PlaySE(SE_SELECT);
        PrintTextToBottomBox(RECIEVED_ITEM);
        BirchCase_GiveItem();
        gTasks[taskId].func = Task_ItemCraftRecievedItem;
        return;
    }
    if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        PrintTextToBottomBox(CHOOSE_ITEM);
        gTasks[taskId].func = Task_ItemCraftMain;
        return;
    }
}

/* Main Grid Based Movement Control Flow*/
static void Task_ItemCraftMain(u8 taskId)
{
    u16 oldPosition = sItemCraftDataPtr->cursorPosition;
    if(JOY_NEW(DPAD_UP))
    {
        PlaySE(SE_SELECT);
        if(sItemCraftDataPtr->cursorPosition != 0) // se non si è in cima si sale di 1
        {
            sItemCraftDataPtr->cursorPosition--;
        }
        ChangePositionUpdateSpriteAnims(oldPosition, taskId);
        return;
    }
    if(JOY_NEW(DPAD_DOWN))
    {
        PlaySE(SE_SELECT);
        if(sItemCraftDataPtr->cursorPosition != CURSOR_MAX_POSITION) // se non si è in al fondo si scende di 1
        {
            sItemCraftDataPtr->cursorPosition++;
        }
        ChangePositionUpdateSpriteAnims(oldPosition, taskId);
        return;
    }
    if(JOY_NEW(A_BUTTON))
    {
        PlaySE(SE_SELECT);
        PrintTextToBottomBar(CONFIRM_SELECTION);
        gTasks[taskId].func = Task_ItemCraftConfirmSelection;
        return;
    }
}


#undef WINDOW_ITEM_CRAFT