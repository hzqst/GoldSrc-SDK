#pragma once

// Which studio model textures the engine remaps to the player's topcolor and bottomcolor, and which palette entries
// of them. The engine's studio renderer remaps by this rule, and GameUI previews the colors of a player model only when
// one of its textures follows it.

// The palette entries remapped on a DM_Base.bmp texture: topcolor from STUDIO_REMAP_LOW to STUDIO_REMAP_MID, bottomcolor
// from STUDIO_REMAP_MID + 1 to STUDIO_REMAP_HIGH.
#define STUDIO_REMAP_LOW  160
#define STUDIO_REMAP_MID  191
#define STUDIO_REMAP_HIGH 223
constexpr int STUDIO_REMAP_PALETTE_COLORS = 256;

// The names that give their own entries: Remap<c>_<low>_<mid>.bmp, where <c> is c or C and bottomcolor is not remapped
// (high is 0), and Remap<c>_<low>_<mid>_<high>.bmp; each number is three digits.
#define STUDIO_REMAP_PREFIX           "Remap"
#define STUDIO_REMAP_LOW_MID_LENGTH   18
#define STUDIO_REMAP_LOW_HIGH_LENGTH  22
#define STUDIO_REMAP_KIND_OFFSET      5
#define STUDIO_REMAP_LOW_OFFSET       7
#define STUDIO_REMAP_MID_OFFSET       11
#define STUDIO_REMAP_HIGH_OFFSET      15
#define STUDIO_REMAP_NUMBER_LENGTH    3

inline char Studio_RemapLowerCase(char ch)
{
    return (ch >= 'A' && ch <= 'Z') ? static_cast<char>(ch - 'A' + 'a') : ch;
}

inline bool Studio_RemapEqualsNoCase(const char* a, const char* b, int length)
{
    for (int i = 0; i < length; ++i)
    {
        if (Studio_RemapLowerCase(a[i]) != Studio_RemapLowerCase(b[i]))
            return false;

        if (!a[i])
            return true;
    }

    return true;
}

// The decimal number of the STUDIO_REMAP_NUMBER_LENGTH characters at text: an optional minus sign, then the digits up
// to the first other character.
inline int Studio_RemapParseNumber(const char* text)
{
    int sign  = 1;
    int value = 0;
    int i     = 0;

    if (text[0] == '-')
    {
        sign = -1;
        ++i;
    }

    for (; i < STUDIO_REMAP_NUMBER_LENGTH && text[i] >= '0' && text[i] <= '9'; ++i)
        value = value * 10 + (text[i] - '0');

    return value * sign;
}

// Whether the texture named name is remapped; if it is, low, mid and high receive its palette entries (high is 0 when
// bottomcolor is not remapped). name is null terminated.
inline bool Studio_IsRemapTexture(const char* name, int* low, int* mid, int* high)
{
    if (Studio_RemapEqualsNoCase(name, "DM_Base.bmp", sizeof("DM_Base.bmp")))
    {
        *low  = STUDIO_REMAP_LOW;
        *mid  = STUDIO_REMAP_MID;
        *high = STUDIO_REMAP_HIGH;
        return true;
    }

    if (!Studio_RemapEqualsNoCase(name, STUDIO_REMAP_PREFIX, sizeof(STUDIO_REMAP_PREFIX) - 1))
        return false;

    int length = 0;

    while (length <= STUDIO_REMAP_LOW_HIGH_LENGTH && name[length])
        ++length;

    const char kind = Studio_RemapLowerCase(name[STUDIO_REMAP_KIND_OFFSET]);

    const bool bLowMid  = length == STUDIO_REMAP_LOW_MID_LENGTH && kind == 'c';
    const bool bLowHigh = length == STUDIO_REMAP_LOW_HIGH_LENGTH;

    if (!bLowMid && !bLowHigh)
        return false;

    *low  = Studio_RemapParseNumber(&name[STUDIO_REMAP_LOW_OFFSET]);
    *mid  = Studio_RemapParseNumber(&name[STUDIO_REMAP_MID_OFFSET]);
    *high = bLowHigh ? Studio_RemapParseNumber(&name[STUDIO_REMAP_HIGH_OFFSET]) : 0;
    return *low >= 0 && *low <= *mid && *mid < STUDIO_REMAP_PALETTE_COLORS &&
        (*high == 0 || (*high > *mid && *high < STUDIO_REMAP_PALETTE_COLORS));
}
