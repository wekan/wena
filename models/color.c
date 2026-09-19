#include "color.h"

#include <string.h>

static const WenaColorContract colors[] = {
    {"belize", "#2980b9"}, {"nephritis", "#27ae60"},
    {"pomegranate", "#c0392b"}, {"pumpkin", "#e67e22"},
    {"wisteria", "#8e44ad"}, {"moderatepink", "#cd5a91"},
    {"strongcyan", "#00aecc"}, {"limegreen", "#4bbf6b"},
    {"midnight", "#2c3e50"}, {"dark", "#333333"},
    {"relax", "#568ba2"}, {"corteza", "#568ba2"},
    {"natural", "#6b8e23"}, {"modern", "#2980b9"},
    {"moderndark", "#263238"}, {"exodark", "#1f2933"},
    {"cleandark", "#263238"}, {"cleanlight", "#e8f3fa"},
    {"clearblue", "#2980b9"}, {"cleargreen", "#27ae60"},
    {"clearorange", "#e67e22"}, {"clearpink", "#cd5a91"},
    {"clearpurple", "#8e44ad"}, {"clearred", "#c0392b"},
    {"appleglasspastel", "#568ba2"},
    {"white", "#ffffff"}, {"green", "#3cb500"},
    {"yellow", "#fad900"}, {"orange", "#ff9f19"},
    {"red", "#eb4646"}, {"purple", "#a632db"},
    {"blue", "#0079bf"}, {"sky", "#00c2e0"},
    {"lime", "#51e898"}, {"pink", "#ff78cb"},
    {"black", "#4d4d4d"}, {"silver", "#c0c0c0"},
    {"peachpuff", "#ffdab9"}, {"crimson", "#dc143c"},
    {"plum", "#dda0dd"}, {"darkgreen", "#006400"},
    {"slateblue", "#6a5acd"}, {"magenta", "#ff00ff"},
    {"gold", "#ffd700"}, {"navy", "#000080"},
    {"gray", "#808080"}, {"saddlebrown", "#8b4513"},
    {"paleturquoise", "#afeeee"}, {"mistyrose", "#ffe4e1"},
    {"indigo", "#4b0082"}
};

/* WCAG 2.2 relative luminance, linear sRGB rounded to 1/10000.
 * https://www.w3.org/TR/WCAG22/#dfn-relative-luminance
 * x = channel / 255; x <= .04045 ? x / 12.92 : ((x+.055)/1.055)^2.4
 * All following integer products fit even a 32-bit unsigned long. */
static const unsigned int linear_srgb[256] = {
    0u, 3u, 6u, 9u, 12u, 15u, 18u, 21u,
    24u, 27u, 30u, 33u, 37u, 40u, 44u, 48u,
    52u, 56u, 60u, 65u, 70u, 75u, 80u, 86u,
    91u, 97u, 103u, 110u, 116u, 123u, 130u, 137u,
    144u, 152u, 160u, 168u, 176u, 185u, 194u, 203u,
    212u, 222u, 232u, 242u, 252u, 262u, 273u, 284u,
    296u, 307u, 319u, 331u, 343u, 356u, 369u, 382u,
    395u, 409u, 423u, 437u, 452u, 467u, 482u, 497u,
    513u, 529u, 545u, 561u, 578u, 595u, 612u, 630u,
    648u, 666u, 685u, 704u, 723u, 742u, 762u, 782u,
    802u, 823u, 844u, 865u, 887u, 908u, 931u, 953u,
    976u, 999u, 1022u, 1046u, 1070u, 1095u, 1119u, 1144u,
    1170u, 1195u, 1221u, 1248u, 1274u, 1301u, 1329u, 1356u,
    1384u, 1413u, 1441u, 1470u, 1500u, 1529u, 1559u, 1590u,
    1620u, 1651u, 1683u, 1714u, 1746u, 1779u, 1812u, 1845u,
    1878u, 1912u, 1946u, 1981u, 2016u, 2051u, 2086u, 2122u,
    2159u, 2195u, 2232u, 2270u, 2307u, 2346u, 2384u, 2423u,
    2462u, 2502u, 2542u, 2582u, 2623u, 2664u, 2705u, 2747u,
    2789u, 2831u, 2874u, 2918u, 2961u, 3005u, 3050u, 3095u,
    3140u, 3185u, 3231u, 3278u, 3325u, 3372u, 3419u, 3467u,
    3515u, 3564u, 3613u, 3663u, 3712u, 3763u, 3813u, 3864u,
    3916u, 3968u, 4020u, 4072u, 4125u, 4179u, 4233u, 4287u,
    4342u, 4397u, 4452u, 4508u, 4564u, 4621u, 4678u, 4735u,
    4793u, 4851u, 4910u, 4969u, 5029u, 5089u, 5149u, 5210u,
    5271u, 5333u, 5395u, 5457u, 5520u, 5583u, 5647u, 5711u,
    5776u, 5841u, 5906u, 5972u, 6038u, 6105u, 6172u, 6240u,
    6308u, 6376u, 6445u, 6514u, 6584u, 6654u, 6724u, 6795u,
    6867u, 6939u, 7011u, 7084u, 7157u, 7231u, 7305u, 7379u,
    7454u, 7529u, 7605u, 7682u, 7758u, 7835u, 7913u, 7991u,
    8070u, 8148u, 8228u, 8308u, 8388u, 8469u, 8550u, 8632u,
    8714u, 8796u, 8879u, 8963u, 9047u, 9131u, 9216u, 9301u,
    9387u, 9473u, 9560u, 9647u, 9734u, 9823u, 9911u, 10000u
};

const WenaColorContract *wena_colors(size_t *count)
{
    if (count != NULL) *count = 25;
    return colors + 25;
}

const WenaColorContract *wena_color_contracts(size_t *count)
{
    if (count != NULL) *count = sizeof(colors) / sizeof(colors[0]);
    return colors;
}

static int hex_digit(unsigned char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int wena_color_rgb(const char *text, unsigned char rgb[3])
{
    size_t length;
    size_t index;
    const char *hex;
    unsigned char parsed[3];
    int high;
    int low;
    if (text == NULL || rgb == NULL) return 0;
    for (length = 0; length < WENA_COLOR_CAPACITY; ++length) {
        if (text[length] == '\0') break;
    }
    if (length == WENA_COLOR_CAPACITY) return 0;
    hex = NULL;
    if (length == 0) hex = "#ffffff";
    else if (length == 7 && text[0] == '#') hex = text;
    else {
        for (index = 25; index < sizeof(colors) / sizeof(colors[0]); ++index) {
            if (strcmp(text, colors[index].name) == 0) {
                hex = colors[index].rgb;
                break;
            }
        }
    }
    if (hex == NULL) return 0;
    for (index = 0; index < 3; ++index) {
        high = hex_digit((unsigned char)hex[1 + index * 2]);
        low = hex_digit((unsigned char)hex[2 + index * 2]);
        if (high < 0 || low < 0) return 0;
        parsed[index] = (unsigned char)(high * 16 + low);
    }
    memcpy(rgb, parsed, sizeof(parsed));
    return 1;
}

int wena_color_valid(const char *text)
{
    unsigned char rgb[3];
    return wena_color_rgb(text, rgb);
}

int wena_color_foreground(const char *text, unsigned char rgb[3])
{
    unsigned char background[3];
    unsigned char value;
    unsigned long luminance;
    unsigned long adjusted;
    if (rgb == NULL || !wena_color_rgb(text, background)) return 0;
    luminance = (2126UL * linear_srgb[background[0]] +
                 7152UL * linear_srgb[background[1]] +
                  722UL * linear_srgb[background[2]]) / 10000UL;
    adjusted = luminance + 500UL;
    value = adjusted * adjusted >= 5250000UL ? 0 : 255;
    rgb[0] = value;
    rgb[1] = value;
    rgb[2] = value;
    return 1;
}
