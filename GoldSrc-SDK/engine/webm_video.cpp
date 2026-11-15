#include "webm_video.h"

#include "conprint.h"
#include "mathlib.h"
#include "sound.h"
#include "sys.h"

#include "../external/SDL2/SDL.h"
#ifndef SW_RENDERER
#include "../external/SDL2/SDL_opengl.h"
#endif

#include <webm/mkvparser/mkvparser.h>
#include <webm/mkvparser/mkvreader.h>
#include <vorbis/codec.h>
#if defined( _MSC_VER )
#pragma warning( push )
#pragma warning( disable : 4505 )
#endif
#include <vpx/vp8dx.h>
#include <vpx/vpx_decoder.h>
#if defined( _MSC_VER )
#pragma warning( pop )
#endif

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"engine\\webm_video.cpp" )

void Sys_Sleep( int msec );

void yuv420_2_rgb8888( unsigned char* destination, const unsigned char* y, const unsigned char* u,
	const unsigned char* v, std::int32_t width, std::int32_t height, std::int32_t ySpan, std::int32_t uvSpan,
	std::int32_t destinationSpan, const std::uint32_t* tables, std::int32_t dither );
void yuv422_2_rgb8888( std::uint8_t* destination, const std::uint8_t* y, const std::uint8_t* u, const std::uint8_t* v,
	std::int32_t width, std::int32_t height, std::int32_t ySpan, std::int32_t uvSpan, std::int32_t destinationSpan,
	const std::uint32_t* tables, std::int32_t dither );
void yuv444_2_rgb8888( std::uint8_t* destination, const std::uint8_t* y, const std::uint8_t* u, const std::uint8_t* v,
	std::int32_t width, std::int32_t height, std::int32_t ySpan, std::int32_t uvSpan, std::int32_t destinationSpan,
	const std::uint32_t* tables, std::int32_t dither );

static const std::uint32_t yuv2rgb565_table[] = {
	0x7FFFFFEDu,
	0x7FFFFFEFu,
	0x7FFFFFF0u,
	0x7FFFFFF1u,
	0x7FFFFFF2u,
	0x7FFFFFF3u,
	0x7FFFFFF4u,
	0x7FFFFFF6u,
	0x7FFFFFF7u,
	0x7FFFFFF8u,
	0x7FFFFFF9u,
	0x7FFFFFFAu,
	0x7FFFFFFBu,
	0x7FFFFFFDu,
	0x7FFFFFFEu,
	0x7FFFFFFFu,
	0x80000000u,
	0x80400801u,
	0x80A01002u,
	0x80E01803u,
	0x81202805u,
	0x81803006u,
	0x81C03807u,
	0x82004008u,
	0x82604809u,
	0x82A0500Au,
	0x82E0600Cu,
	0x8340680Du,
	0x8380700Eu,
	0x83C0780Fu,
	0x84208010u,
	0x84608811u,
	0x84A09813u,
	0x8500A014u,
	0x8540A815u,
	0x8580B016u,
	0x85E0B817u,
	0x8620C018u,
	0x8660D01Au,
	0x86C0D81Bu,
	0x8700E01Cu,
	0x8740E81Du,
	0x87A0F01Eu,
	0x87E0F81Fu,
	0x88210821u,
	0x88811022u,
	0x88C11823u,
	0x89012024u,
	0x89412825u,
	0x89A13026u,
	0x89E14028u,
	0x8A214829u,
	0x8A81502Au,
	0x8AC1582Bu,
	0x8B01602Cu,
	0x8B61682Du,
	0x8BA1782Fu,
	0x8BE18030u,
	0x8C418831u,
	0x8C819032u,
	0x8CC19833u,
	0x8D21A034u,
	0x8D61B036u,
	0x8DA1B837u,
	0x8E01C038u,
	0x8E41C839u,
	0x8E81D03Au,
	0x8EE1D83Bu,
	0x8F21E83Du,
	0x8F61F03Eu,
	0x8FC1F83Fu,
	0x90020040u,
	0x90420841u,
	0x90A21042u,
	0x90E22044u,
	0x91222845u,
	0x91823046u,
	0x91C23847u,
	0x92024048u,
	0x92624849u,
	0x92A2504Au,
	0x92E2604Cu,
	0x9342684Du,
	0x9382704Eu,
	0x93C2784Fu,
	0x94228050u,
	0x94628851u,
	0x94A29853u,
	0x9502A054u,
	0x9542A855u,
	0x9582B056u,
	0x95E2B857u,
	0x9622C058u,
	0x9662D05Au,
	0x96C2D85Bu,
	0x9702E05Cu,
	0x9742E85Du,
	0x97A2F05Eu,
	0x97E2F85Fu,
	0x98230861u,
	0x98831062u,
	0x98C31863u,
	0x99032064u,
	0x99632865u,
	0x99A33066u,
	0x99E34068u,
	0x9A434869u,
	0x9A83506Au,
	0x9AC3586Bu,
	0x9B23606Cu,
	0x9B63686Du,
	0x9BA3786Fu,
	0x9BE38070u,
	0x9C438871u,
	0x9C839072u,
	0x9CC39873u,
	0x9D23A074u,
	0x9D63B076u,
	0x9DA3B877u,
	0x9E03C078u,
	0x9E43C879u,
	0x9E83D07Au,
	0x9EE3D87Bu,
	0x9F23E87Du,
	0x9F63F07Eu,
	0x9FC3F87Fu,
	0xA0040080u,
	0xA0440881u,
	0xA0A41082u,
	0xA0E42084u,
	0xA1242885u,
	0xA1843086u,
	0xA1C43887u,
	0xA2044088u,
	0xA2644889u,
	0xA2A4588Bu,
	0xA2E4608Cu,
	0xA344688Du,
	0xA384708Eu,
	0xA3C4788Fu,
	0xA4248090u,
	0xA4649092u,
	0xA4A49893u,
	0xA504A094u,
	0xA544A895u,
	0xA584B096u,
	0xA5E4B897u,
	0xA624C098u,
	0xA664D09Au,
	0xA6C4D89Bu,
	0xA704E09Cu,
	0xA744E89Du,
	0xA7A4F09Eu,
	0xA7E4F89Fu,
	0xA82508A1u,
	0xA88510A2u,
	0xA8C518A3u,
	0xA90520A4u,
	0xA96528A5u,
	0xA9A530A6u,
	0xA9E540A8u,
	0xAA4548A9u,
	0xAA8550AAu,
	0xAAC558ABu,
	0xAB2560ACu,
	0xAB6568ADu,
	0xABA578AFu,
	0xAC0580B0u,
	0xAC4588B1u,
	0xAC8590B2u,
	0xACE598B3u,
	0xAD25A0B4u,
	0xAD65B0B6u,
	0xADA5B8B7u,
	0xAE05C0B8u,
	0xAE45C8B9u,
	0xAE85D0BAu,
	0xAEE5D8BBu,
	0xAF25E8BDu,
	0xAF65F0BEu,
	0xAFC5F8BFu,
	0xB00600C0u,
	0xB04608C1u,
	0xB0A610C2u,
	0xB0E620C4u,
	0xB12628C5u,
	0xB18630C6u,
	0xB1C638C7u,
	0xB20640C8u,
	0xB26648C9u,
	0xB2A658CBu,
	0xB2E660CCu,
	0xB34668CDu,
	0xB38670CEu,
	0xB3C678CFu,
	0xB42680D0u,
	0xB46690D2u,
	0xB4A698D3u,
	0xB506A0D4u,
	0xB546A8D5u,
	0xB586B0D6u,
	0xB5E6B8D7u,
	0xB626C8D9u,
	0xB666D0DAu,
	0xB6C6D8DBu,
	0xB706E0DCu,
	0xB746E8DDu,
	0xB7A6F0DEu,
	0xB7E6F8DFu,
	0xB82708E1u,
	0xB88710E2u,
	0xB8C718E3u,
	0xB90720E4u,
	0xB96728E5u,
	0xB9A730E6u,
	0xB9E740E8u,
	0xBA4748E9u,
	0xBA8750EAu,
	0xBAC758EBu,
	0xBB2760ECu,
	0xBB6768EDu,
	0xBBA778EFu,
	0xBC0780F0u,
	0xBC4788F1u,
	0xBC8790F2u,
	0xBCE798F3u,
	0xBD27A0F4u,
	0xBD67B0F6u,
	0xBDC7B8F7u,
	0xBE07C0F8u,
	0xBE47C8F9u,
	0xBEA7D0FAu,
	0xBEE7D8FBu,
	0xBF27E8FDu,
	0xBF87F0FEu,
	0xBFC7F8FFu,
	0xC0080100u,
	0xC0480901u,
	0xC0A81102u,
	0xC0E82104u,
	0xC0E82104u,
	0xC0E82104u,
	0xC0E82104u,
	0xC0E82104u,
	0xC0E82104u,
	0xC0E82104u,
	0xC0E82104u,
	0xC0E82104u,
	0xC0E82104u,
	0xC0E82104u,
	0xC0E82104u,
	0xC0E82104u,
	0xC0E82104u,
	0xC0E82104u,
	0xC0E82104u,
	0xC0E82104u,
	0x0C400103u,
	0x0C200105u,
	0x0C200107u,
	0x0C000109u,
	0x0BE0010Bu,
	0x0BC0010Du,
	0x0BA0010Fu,
	0x0BA00111u,
	0x0B800113u,
	0x0B600115u,
	0x0B400117u,
	0x0B400119u,
	0x0B20011Bu,
	0x0B00011Du,
	0x0AE0011Fu,
	0x0AE00121u,
	0x0AC00123u,
	0x0AA00125u,
	0x0A800127u,
	0x0A600129u,
	0x0A60012Bu,
	0x0A40012Du,
	0x0A20012Fu,
	0x0A000131u,
	0x0A000132u,
	0x09E00134u,
	0x09C00136u,
	0x09A00138u,
	0x09A0013Au,
	0x0980013Cu,
	0x0960013Eu,
	0x09400140u,
	0x09400142u,
	0x09200144u,
	0x09000146u,
	0x08E00148u,
	0x08C0014Au,
	0x08C0014Cu,
	0x08A0014Eu,
	0x08800150u,
	0x08600152u,
	0x08600154u,
	0x08400156u,
	0x08200158u,
	0x0800015Au,
	0x0800015Cu,
	0x07E0015Eu,
	0x07C00160u,
	0x07A00162u,
	0x07A00164u,
	0x07800166u,
	0x07600168u,
	0x0740016Au,
	0x0720016Cu,
	0x0720016Eu,
	0x07000170u,
	0x06E00172u,
	0x06C00174u,
	0x06C00176u,
	0x06A00178u,
	0x0680017Au,
	0x0660017Cu,
	0x0660017Eu,
	0x06400180u,
	0x06200182u,
	0x06000184u,
	0x05E00185u,
	0x05E00187u,
	0x05C00189u,
	0x05A0018Bu,
	0x0580018Du,
	0x0580018Fu,
	0x05600191u,
	0x05400193u,
	0x05200195u,
	0x05200197u,
	0x05000199u,
	0x04E0019Bu,
	0x04C0019Du,
	0x04C0019Fu,
	0x04A001A1u,
	0x048001A3u,
	0x046001A5u,
	0x044001A7u,
	0x044001A9u,
	0x042001ABu,
	0x040001ADu,
	0x03E001AFu,
	0x03E001B1u,
	0x03C001B3u,
	0x03A001B5u,
	0x038001B7u,
	0x038001B9u,
	0x036001BBu,
	0x034001BDu,
	0x032001BFu,
	0x032001C1u,
	0x030001C3u,
	0x02E001C5u,
	0x02C001C7u,
	0x02A001C9u,
	0x02A001CBu,
	0x028001CDu,
	0x026001CFu,
	0x024001D1u,
	0x024001D3u,
	0x022001D5u,
	0x020001D7u,
	0x01E001D8u,
	0x01E001DAu,
	0x01C001DCu,
	0x01A001DEu,
	0x018001E0u,
	0x016001E2u,
	0x016001E4u,
	0x014001E6u,
	0x012001E8u,
	0x010001EAu,
	0x010001ECu,
	0x00E001EEu,
	0x00C001F0u,
	0x00A001F2u,
	0x00A001F4u,
	0x008001F6u,
	0x006001F8u,
	0x004001FAu,
	0x004001FCu,
	0x002001FEu,
	0x00000200u,
	0xFFE00202u,
	0xFFC00204u,
	0xFFC00206u,
	0xFFA00208u,
	0xFF80020Au,
	0xFF60020Cu,
	0xFF60020Eu,
	0xFF400210u,
	0xFF200212u,
	0xFF000214u,
	0xFF000216u,
	0xFEE00218u,
	0xFEC0021Au,
	0xFEA0021Cu,
	0xFEA0021Eu,
	0xFE800220u,
	0xFE600222u,
	0xFE400224u,
	0xFE200226u,
	0xFE200228u,
	0xFE000229u,
	0xFDE0022Bu,
	0xFDC0022Du,
	0xFDC0022Fu,
	0xFDA00231u,
	0xFD800233u,
	0xFD600235u,
	0xFD600237u,
	0xFD400239u,
	0xFD20023Bu,
	0xFD00023Du,
	0xFCE0023Fu,
	0xFCE00241u,
	0xFCC00243u,
	0xFCA00245u,
	0xFC800247u,
	0xFC800249u,
	0xFC60024Bu,
	0xFC40024Du,
	0xFC20024Fu,
	0xFC200251u,
	0xFC000253u,
	0xFBE00255u,
	0xFBC00257u,
	0xFBC00259u,
	0xFBA0025Bu,
	0xFB80025Du,
	0xFB60025Fu,
	0xFB400261u,
	0xFB400263u,
	0xFB200265u,
	0xFB000267u,
	0xFAE00269u,
	0xFAE0026Bu,
	0xFAC0026Du,
	0xFAA0026Fu,
	0xFA800271u,
	0xFA800273u,
	0xFA600275u,
	0xFA400277u,
	0xFA200279u,
	0xFA20027Bu,
	0xFA00027Cu,
	0xF9E0027Eu,
	0xF9C00280u,
	0xF9A00282u,
	0xF9A00284u,
	0xF9800286u,
	0xF9600288u,
	0xF940028Au,
	0xF940028Cu,
	0xF920028Eu,
	0xF9000290u,
	0xF8E00292u,
	0xF8E00294u,
	0xF8C00296u,
	0xF8A00298u,
	0xF880029Au,
	0xF860029Cu,
	0xF860029Eu,
	0xF84002A0u,
	0xF82002A2u,
	0xF80002A4u,
	0xF80002A6u,
	0xF7E002A8u,
	0xF7C002AAu,
	0xF7A002ACu,
	0xF7A002AEu,
	0xF78002B0u,
	0xF76002B2u,
	0xF74002B4u,
	0xF74002B6u,
	0xF72002B8u,
	0xF70002BAu,
	0xF6E002BCu,
	0xF6C002BEu,
	0xF6C002C0u,
	0xF6A002C2u,
	0xF68002C4u,
	0xF66002C6u,
	0xF66002C8u,
	0xF64002CAu,
	0xF62002CCu,
	0xF60002CEu,
	0xF60002CFu,
	0xF5E002D1u,
	0xF5C002D3u,
	0xF5A002D5u,
	0xF5A002D7u,
	0xF58002D9u,
	0xF56002DBu,
	0xF54002DDu,
	0xF52002DFu,
	0xF52002E1u,
	0xF50002E3u,
	0xF4E002E5u,
	0xF4C002E7u,
	0xF4C002E9u,
	0xF4A002EBu,
	0xF48002EDu,
	0xF46002EFu,
	0xF46002F1u,
	0xF44002F3u,
	0xF42002F5u,
	0xF40002F7u,
	0xF3E002F9u,
	0xF3E002FBu,
	0x1A09A000u,
	0x19E9A800u,
	0x19A9B800u,
	0x1969C800u,
	0x1949D000u,
	0x1909E000u,
	0x18C9E800u,
	0x18A9F800u,
	0x186A0000u,
	0x182A1000u,
	0x180A2000u,
	0x17CA2800u,
	0x17AA3800u,
	0x176A4000u,
	0x172A5000u,
	0x170A6000u,
	0x16CA6800u,
	0x168A7800u,
	0x166A8000u,
	0x162A9000u,
	0x160AA000u,
	0x15CAA800u,
	0x158AB800u,
	0x156AC000u,
	0x152AD000u,
	0x14EAE000u,
	0x14CAE800u,
	0x148AF800u,
	0x146B0000u,
	0x142B1000u,
	0x13EB2000u,
	0x13CB2800u,
	0x138B3800u,
	0x134B4000u,
	0x132B5000u,
	0x12EB6000u,
	0x12CB6800u,
	0x128B7800u,
	0x124B8000u,
	0x122B9000u,
	0x11EBA000u,
	0x11ABA800u,
	0x118BB800u,
	0x114BC000u,
	0x112BD000u,
	0x10EBE000u,
	0x10ABE800u,
	0x108BF800u,
	0x104C0000u,
	0x100C1000u,
	0x0FEC2000u,
	0x0FAC2800u,
	0x0F8C3800u,
	0x0F4C4000u,
	0x0F0C5000u,
	0x0EEC5800u,
	0x0EAC6800u,
	0x0E6C7800u,
	0x0E4C8000u,
	0x0E0C9000u,
	0x0DEC9800u,
	0x0DACA800u,
	0x0D6CB800u,
	0x0D4CC000u,
	0x0D0CD000u,
	0x0CCCD800u,
	0x0CACE800u,
	0x0C6CF800u,
	0x0C4D0000u,
	0x0C0D1000u,
	0x0BCD1800u,
	0x0BAD2800u,
	0x0B6D3800u,
	0x0B2D4000u,
	0x0B0D5000u,
	0x0ACD5800u,
	0x0AAD6800u,
	0x0A6D7800u,
	0x0A2D8000u,
	0x0A0D9000u,
	0x09CD9800u,
	0x098DA800u,
	0x096DB800u,
	0x092DC000u,
	0x090DD000u,
	0x08CDD800u,
	0x088DE800u,
	0x086DF800u,
	0x082E0000u,
	0x07EE1000u,
	0x07CE1800u,
	0x078E2800u,
	0x076E3800u,
	0x072E4000u,
	0x06EE5000u,
	0x06CE5800u,
	0x068E6800u,
	0x064E7800u,
	0x062E8000u,
	0x05EE9000u,
	0x05CE9800u,
	0x058EA800u,
	0x054EB800u,
	0x052EC000u,
	0x04EED000u,
	0x04AED800u,
	0x048EE800u,
	0x044EF000u,
	0x042F0000u,
	0x03EF1000u,
	0x03AF1800u,
	0x038F2800u,
	0x034F3000u,
	0x030F4000u,
	0x02EF5000u,
	0x02AF5800u,
	0x028F6800u,
	0x024F7000u,
	0x020F8000u,
	0x01EF9000u,
	0x01AF9800u,
	0x016FA800u,
	0x014FB000u,
	0x010FC000u,
	0x00EFD000u,
	0x00AFD800u,
	0x006FE800u,
	0x004FF000u,
	0x00100000u,
	0xFFD01000u,
	0xFFB01800u,
	0xFF702800u,
	0xFF303000u,
	0xFF104000u,
	0xFED05000u,
	0xFEB05800u,
	0xFE706800u,
	0xFE307000u,
	0xFE108000u,
	0xFDD09000u,
	0xFD909800u,
	0xFD70A800u,
	0xFD30B000u,
	0xFD10C000u,
	0xFCD0D000u,
	0xFC90D800u,
	0xFC70E800u,
	0xFC30F000u,
	0xFBF10000u,
	0xFBD11000u,
	0xFB911800u,
	0xFB712800u,
	0xFB313000u,
	0xFAF14000u,
	0xFAD14800u,
	0xFA915800u,
	0xFA516800u,
	0xFA317000u,
	0xF9F18000u,
	0xF9D18800u,
	0xF9919800u,
	0xF951A800u,
	0xF931B000u,
	0xF8F1C000u,
	0xF8B1C800u,
	0xF891D800u,
	0xF851E800u,
	0xF831F000u,
	0xF7F20000u,
	0xF7B20800u,
	0xF7921800u,
	0xF7522800u,
	0xF7123000u,
	0xF6F24000u,
	0xF6B24800u,
	0xF6925800u,
	0xF6526800u,
	0xF6127000u,
	0xF5F28000u,
	0xF5B28800u,
	0xF5729800u,
	0xF552A800u,
	0xF512B000u,
	0xF4F2C000u,
	0xF4B2C800u,
	0xF472D800u,
	0xF452E800u,
	0xF412F000u,
	0xF3D30000u,
	0xF3B30800u,
	0xF3731800u,
	0xF3532800u,
	0xF3133000u,
	0xF2D34000u,
	0xF2B34800u,
	0xF2735800u,
	0xF2336800u,
	0xF2137000u,
	0xF1D38000u,
	0xF1B38800u,
	0xF1739800u,
	0xF133A800u,
	0xF113B000u,
	0xF0D3C000u,
	0xF093C800u,
	0xF073D800u,
	0xF033E000u,
	0xF013F000u,
	0xEFD40000u,
	0xEF940800u,
	0xEF741800u,
	0xEF342000u,
	0xEEF43000u,
	0xEED44000u,
	0xEE944800u,
	0xEE745800u,
	0xEE346000u,
	0xEDF47000u,
	0xEDD48000u,
	0xED948800u,
	0xED549800u,
	0xED34A000u,
	0xECF4B000u,
	0xECD4C000u,
	0xEC94C800u,
	0xEC54D800u,
	0xEC34E000u,
	0xEBF4F000u,
	0xEBB50000u,
	0xEB950800u,
	0xEB551800u,
	0xEB352000u,
	0xEAF53000u,
	0xEAB54000u,
	0xEA954800u,
	0xEA555800u,
	0xEA156000u,
	0xE9F57000u,
	0xE9B58000u,
	0xE9958800u,
	0xE9559800u,
	0xE915A000u,
	0xE8F5B000u,
	0xE8B5C000u,
	0xE875C800u,
	0xE855D800u,
	0xE815E000u,
	0xE7F5F000u,
	0xE7B60000u,
	0xE7760800u,
	0xE7561800u,
	0xE7162000u,
	0xE6D63000u,
	0xE6B64000u,
	0xE6764800u,
	0xE6365800u,
};

static_assert( sizeof( yuv2rgb565_table ) == 768 * sizeof( std::uint32_t ), "yuv table" );
#if defined( _WIN32 )
static_assert( sizeof( WebMFrame ) == 32, "WebMFrame" );
#else
static_assert( sizeof( WebMFrame ) == 24, "WebMFrame" );
#endif
static_assert( sizeof( WebMDemuxer ) == 44, "WebMDemuxer" );
static_assert( sizeof( VPXDecoder ) == 16, "VPXDecoder" );
static_assert( sizeof( VPXDecoder::Image ) == 44, "VPXDecoder::Image" );
static_assert( sizeof( AudioVorbisDecoder ) == 12, "AudioVorbisDecoder" );

constexpr unsigned int kVpxMaxThreads = 8;
constexpr unsigned int kVpxFmtAcceptedMask = VPX_IMG_FMT_PLANAR | VPX_IMG_FMT_HAS_ALPHA | VPX_IMG_FMT_HIGHBITDEPTH;
constexpr int kVorbisScratchSamples = 4096;
constexpr unsigned int kPcmCountLimit = 0x3F800000u;
constexpr double kNanoseconds = 1000000000.0;
constexpr int kPlaybackChannel = 3;
constexpr int kPlaybackPitch = 100;

struct VorbisDecoder
{
	vorbis_info info;
	vorbis_dsp_state dspState;
	vorbis_block block;
	ogg_packet op;
	bool hasDSPState;
	bool hasBlock;
};

static void ReleaseVorbis( VorbisDecoder* decoder )
{
	if ( decoder == nullptr )
		return;
	if ( decoder->hasBlock )
		vorbis_block_clear( &decoder->block );
	if ( decoder->hasDSPState )
		vorbis_dsp_clear( &decoder->dspState );
	vorbis_info_clear( &decoder->info );
	delete decoder;
}

#if !defined( _WIN32 )
static int TruncateFloat( double value )
{
	return static_cast<int>( static_cast<float>( value ) );
}
#endif

static bool ReadXiphHeaders( const unsigned char* data, unsigned int size, ogg_packet packets[3] )
{
	unsigned int index = 1;
	int sizes[3] = { 0, 0, 0 };
	int* cursor = sizes;
	for ( ;; )
	{
		int lace = data[index++];
		int sum = *cursor + lace;
		*cursor = sum;
		if ( static_cast<unsigned char>( lace ) == 0xFF )
		{
			while ( size != index )
			{
				const int continuation = data[index++];
				sum += continuation;
				*cursor = sum;
				if ( static_cast<unsigned char>( continuation ) != 0xFF )
					goto nextHeader;
			}
			return false;
		}
	nextHeader:
		if ( ++cursor == &sizes[2] )
			break;
		if ( index >= size )
			return false;
	}

	const int first = sizes[0];
	const int second = sizes[1];
	const unsigned int used = static_cast<unsigned int>( first + second ) + index;
	const int third = static_cast<int>( size - used );
	std::memset( packets, 0, sizeof( ogg_packet ) * 3 );
	packets[0].packet = const_cast<unsigned char*>( data + index );
	packets[0].bytes = first;
	packets[0].b_o_s = 1;
	packets[1].packet = const_cast<unsigned char*>( data + index + first );
	packets[1].bytes = second;
	packets[2].packet = const_cast<unsigned char*>( data + index + first + second );
	packets[2].bytes = third;
	return true;
}

WebMFrame::WebMFrame()
{
	m_uBufferSize = 0;
	m_flTime = 0.0;
	m_uBufferCapacity = 0;
	m_pBuffer = nullptr;
	m_bKey = false;
}

WebMFrame::~WebMFrame()
{
	std::free( m_pBuffer );
}

WebMFrame& WebMFrame::operator=( const WebMFrame& other )
{
	m_uBufferSize = other.m_uBufferSize;
	m_uBufferCapacity = other.m_uBufferCapacity;
	m_pBuffer = static_cast<unsigned char*>( std::malloc( static_cast<std::size_t>( m_uBufferCapacity ) ) );
	std::memcpy( m_pBuffer, other.m_pBuffer, static_cast<std::size_t>( m_uBufferCapacity ) );
	m_flTime = other.m_flTime;
	m_bKey = other.m_bKey;
	return *this;
}

WebMDemuxer::WebMDemuxer( mkvparser::IMkvReader* reader, int videoTrack, int audioTrack )
{
	long long position = 0;
	m_pReader = reader;
	m_pSegment = nullptr;
	m_pCluster = nullptr;
	m_pBlock = nullptr;
	m_pBlockEntry = nullptr;
	m_nBlockFrameIndex = 0;
	m_pVideoTrack = nullptr;
	m_eVideoCodec = NO_VIDEO;
	m_pAudioTrack = nullptr;
	m_eAudioCodec = NO_AUDIO;
	m_bIsOpen = false;
	m_bEOS = false;

	long long parsed = 0;
	{
		mkvparser::EBMLHeader header;
		parsed = header.Parse( reader, position );
	}
	if ( parsed != 0 || mkvparser::Segment::CreateInstance( m_pReader, position, m_pSegment ) != 0
		 || m_pSegment->Load() < 0 )
		return;

	const mkvparser::Tracks* tracks = m_pSegment->GetTracks();
	const unsigned int trackCount = tracks->GetTracksCount();
	if ( trackCount != 0 )
	{
		int currentVideo = -1;
		int currentAudio = -1;
		for ( unsigned int index = 0; index < trackCount; ++index )
		{
			const mkvparser::Track* track = tracks->GetTrackByIndex( index );
			const char* codecId = track->GetCodecId();
			if ( codecId == nullptr )
				continue;

			const bool videoSelected = m_pVideoTrack != nullptr && videoTrack == currentVideo;
			if ( !videoSelected && track->GetType() == mkvparser::Track::kVideo )
			{
				if ( std::strcmp( codecId, "V_VP8" ) == 0 )
					m_eVideoCodec = VIDEO_VP8;
				else if ( std::strcmp( codecId, "V_VP9" ) == 0 )
					m_eVideoCodec = VIDEO_VP9;
				if ( std::strcmp( codecId, "V_VP8" ) == 0 || std::strcmp( codecId, "V_VP9" ) == 0
					 || m_eVideoCodec != NO_VIDEO )
					m_pVideoTrack = static_cast<const mkvparser::VideoTrack*>( track );
				++currentVideo;
			}

			if ( m_pAudioTrack != nullptr && audioTrack == currentAudio )
				continue;
			if ( track->GetType() != mkvparser::Track::kAudio )
				continue;
			if ( std::strcmp( codecId, "A_VORBIS" ) == 0 )
				m_eAudioCodec = AUDIO_VORBIS;
			else if ( std::strcmp( codecId, "A_OPUS" ) == 0 )
				m_eAudioCodec = AUDIO_OPUS;
			if ( std::strcmp( codecId, "A_VORBIS" ) == 0 || std::strcmp( codecId, "A_OPUS" ) == 0
				 || m_eAudioCodec != NO_AUDIO )
				m_pAudioTrack = static_cast<const mkvparser::AudioTrack*>( track );
			++currentAudio;
		}
	}

	if ( m_pVideoTrack != nullptr || m_pAudioTrack != nullptr )
		m_bIsOpen = true;
}

WebMDemuxer::~WebMDemuxer()
{
	delete m_pSegment;
}

double WebMDemuxer::GetLength() const
{
	return static_cast<double>( m_pSegment->GetDuration() ) / kNanoseconds;
}

WebMDemuxer::VIDEO_CODEC WebMDemuxer::GetVideoCodec() const
{
	return m_eVideoCodec;
}

int WebMDemuxer::GetWidth() const
{
	return static_cast<int>( m_pVideoTrack->GetWidth() );
}

int WebMDemuxer::GetHeight() const
{
	return static_cast<int>( m_pVideoTrack->GetHeight() );
}

WebMDemuxer::AUDIO_CODEC WebMDemuxer::GetAudioCodec() const
{
	return m_eAudioCodec;
}

const unsigned char* WebMDemuxer::GetAudioExtradata( unsigned int& size ) const
{
	return m_pAudioTrack->GetCodecPrivate( size );
}

double WebMDemuxer::GetSampleRate() const
{
	return m_pAudioTrack->GetSamplingRate();
}

int WebMDemuxer::GetChannels() const
{
	return static_cast<int>( m_pAudioTrack->GetChannels() );
}

int WebMDemuxer::GetAudioDepth() const
{
	return static_cast<int>( m_pAudioTrack->GetBitDepth() );
}

bool WebMDemuxer::ReadFrame( WebMFrame* videoFrame, WebMFrame* audioFrame )
{
	int nVideoTrackNumber = 0;
	int nAudioTrackNumber = 0;
	if ( videoFrame == nullptr )
	{
		if ( audioFrame == nullptr )
			return false;
	}
	else if ( m_pVideoTrack != nullptr )
	{
		nVideoTrackNumber = m_pVideoTrack->GetNumber();
	}

	if ( audioFrame != nullptr )
	{
		if ( m_pAudioTrack != nullptr )
			nAudioTrackNumber = m_pAudioTrack->GetNumber();
		if ( videoFrame != nullptr )
			videoFrame->m_uBufferSize = 0;
		audioFrame->m_uBufferSize = 0;
	}
	else
	{
		videoFrame->m_uBufferSize = 0;
	}

	if ( ( nAudioTrackNumber | nVideoTrackNumber ) == 0 || m_bEOS )
		return false;
	if ( m_pCluster == nullptr )
		m_pCluster = m_pSegment->GetFirst();

	bool advanceCluster = false;
	int clusterStatus = 0;
	for ( ;; )
	{
		for ( ;; )
		{
			if ( m_pBlockEntry == nullptr || advanceCluster || m_pBlockEntry->EOS() )
			{
				if ( m_pBlockEntry != nullptr || advanceCluster )
				{
					m_pCluster = m_pSegment->GetNext( m_pCluster );
					if ( m_pCluster == nullptr || m_pCluster->EOS() )
					{
						m_bEOS = true;
						return false;
					}
				}
				clusterStatus = m_pCluster->GetFirst( m_pBlockEntry );
				break;
			}

			if ( m_pBlock != nullptr && m_nBlockFrameIndex != m_pBlock->GetFrameCount() )
			{
				const int trackNumber = static_cast<int>( m_pBlock->GetTrackNumber() );
				if ( nVideoTrackNumber == trackNumber || trackNumber == nAudioTrackNumber )
				{
					if ( m_pBlockEntry == nullptr )
						return false;
					goto deliver;
				}
			}

			clusterStatus = m_pCluster->GetNext( m_pBlockEntry, m_pBlockEntry );
			if ( m_pBlockEntry != nullptr && !m_pBlockEntry->EOS() )
				break;
			advanceCluster = true;
		}

		if ( clusterStatus != 0 || m_pBlockEntry == nullptr )
			return false;
		m_pBlock = m_pBlockEntry->GetBlock();
		m_nBlockFrameIndex = 0;

	deliver:
		const int trackNumber = static_cast<int>( m_pBlock->GetTrackNumber() );
		if ( nVideoTrackNumber != trackNumber && trackNumber != nAudioTrackNumber )
		{
			advanceCluster = false;
			continue;
		}
		break;
	}

	WebMFrame* frame = videoFrame;
	const int nTrackNumber = static_cast<int>( m_pBlock->GetTrackNumber() );
	if ( nTrackNumber != nVideoTrackNumber )
	{
		ASSERT_AT( nTrackNumber == nVideoTrackNumber || nTrackNumber == nAudioTrackNumber, 585 );
		frame = audioFrame;
	}

	const int frameIndex = m_nBlockFrameIndex;
	++m_nBlockFrameIndex;
	const mkvparser::Block::Frame* blockFrame = &m_pBlock->GetFrame( frameIndex );
	int size = blockFrame->len;
	if ( size > frame->m_uBufferCapacity )
	{
		frame->m_uBufferCapacity = size;
		unsigned char* buffer =
			static_cast<unsigned char*>( std::realloc( frame->m_pBuffer, static_cast<std::size_t>( size ) ) );
		if ( buffer == nullptr )
			return false;
		frame->m_pBuffer = buffer;
		size = blockFrame->len;
	}

	frame->m_uBufferSize = size;
	frame->m_flTime = static_cast<double>( m_pBlock->GetTime( m_pCluster ) ) / kNanoseconds;
	frame->m_bKey = m_pBlock->IsKey();
	return blockFrame->Read( m_pReader, frame->m_pBuffer ) == 0;
}

VPXDecoder::VPXDecoder( const WebMDemuxer& demuxer, unsigned int uThreads )
{
	unsigned int threads = uThreads;
	m_pCtx = nullptr;
	m_pIter = nullptr;
	m_nDelay = 0;
	m_nLastSpace = 0;
	if ( uThreads > kVpxMaxThreads )
		threads = kVpxMaxThreads;
	else if ( uThreads == 0 )
		threads = 1;

	vpx_codec_dec_cfg_t config;
	config.threads = threads;
	config.w = 0;
	config.h = 0;

	vpx_codec_iface_t* iface = nullptr;
	if ( demuxer.GetVideoCodec() == WebMDemuxer::VIDEO_VP8 )
	{
		iface = vpx_codec_vp8_dx();
	}
	else if ( demuxer.GetVideoCodec() == WebMDemuxer::VIDEO_VP9 )
	{
		iface = vpx_codec_vp9_dx();
		m_nDelay = static_cast<int>( threads ) - 1;
	}
	else
	{
		return;
	}

	vpx_codec_flags_t flags = 0;
	if ( m_nDelay > 0 )
		flags = VPX_CODEC_USE_FRAME_THREADING;
	m_pCtx = new vpx_codec_ctx;
	if ( vpx_codec_dec_init( m_pCtx, iface, &config, flags ) != VPX_CODEC_OK )
	{
		delete m_pCtx;
		m_pCtx = nullptr;
	}
}

VPXDecoder::~VPXDecoder()
{
	if ( m_pCtx != nullptr )
	{
		vpx_codec_destroy( m_pCtx );
		delete m_pCtx;
	}
}

bool VPXDecoder::Decode( const WebMFrame& frame )
{
	m_pIter = nullptr;
	return vpx_codec_decode( m_pCtx, frame.m_pBuffer, static_cast<unsigned int>( frame.m_uBufferSize ), nullptr, 0 )
		   == VPX_CODEC_OK;
}

VPXDecoder::IMAGE_ERROR VPXDecoder::GetImage( Image& image )
{
	vpx_image_t* frame = vpx_codec_get_frame( m_pCtx, &m_pIter );
	if ( frame == nullptr )
		return VPX_NO_FRAME;
	if ( frame->cs != 0 )
		m_nLastSpace = frame->cs;
	if ( ( static_cast<unsigned int>( frame->fmt ) & kVpxFmtAcceptedMask ) != VPX_IMG_FMT_PLANAR )
		return VPX_UNSUPPORTED_FRAME;
	if ( frame->stride[0] == 0 || frame->stride[1] == 0 || frame->stride[2] == 0 )
		return VPX_NO_FRAME;

	const bool uvFlip = ( static_cast<unsigned int>( frame->fmt ) & VPX_IMG_FMT_UV_FLIP ) != 0;
	image.w = static_cast<int>( frame->d_w );
	image.h = static_cast<int>( frame->d_h );
	image.cs = m_nLastSpace;
	image.chromaShiftW = static_cast<int>( frame->x_chroma_shift );
	image.chromaShiftH = static_cast<int>( frame->y_chroma_shift );
	image.planes[0] = frame->planes[0];
	image.linesize[0] = frame->stride[0];
	if ( uvFlip )
	{
		image.planes[1] = frame->planes[2];
		image.planes[2] = frame->planes[1];
		image.linesize[1] = frame->stride[2];
		image.linesize[2] = frame->stride[1];
	}
	else
	{
		image.planes[1] = frame->planes[1];
		image.planes[2] = frame->planes[2];
		image.linesize[1] = frame->stride[1];
		image.linesize[2] = frame->stride[2];
	}
	return VPX_NO_ERROR;
}

int VPXDecoder::Image::GetWidth( int plane ) const
{
	if ( plane != 0 )
		return ( w + ( 1 << chromaShiftW ) - 1 ) >> chromaShiftW;
	return w;
}

int VPXDecoder::Image::GetHeight( int plane ) const
{
	if ( plane != 0 )
		return ( h + ( 1 << chromaShiftH ) - 1 ) >> chromaShiftH;
	return h;
}

#ifdef _MSC_VER
#pragma warning( suppress: 26495 )
#endif
AudioVorbisDecoder::AudioVorbisDecoder( const WebMDemuxer& demuxer )
{
	m_pVorbis = nullptr;
	m_nSamples = 0;
	if ( demuxer.GetAudioCodec() == WebMDemuxer::AUDIO_OPUS )
	{
		m_nChannels = demuxer.GetChannels();
		return;
	}
	if ( demuxer.GetAudioCodec() != WebMDemuxer::AUDIO_VORBIS )
		return;

	m_nChannels = demuxer.GetChannels();
	if ( !OpenVorbis( demuxer ) )
		Close();
}

AudioVorbisDecoder::~AudioVorbisDecoder()
{
	ReleaseVorbis( m_pVorbis );
}

bool AudioVorbisDecoder::IsOpen() const
{
	return m_pVorbis != nullptr;
}

bool AudioVorbisDecoder::GetPCMS16( WebMFrame& frame, short* buffer, int& numOutSamples )
{
	if ( m_pVorbis == nullptr )
		return false;
	m_pVorbis->op.packet = frame.m_pBuffer;
	m_pVorbis->op.bytes = frame.m_uBufferSize;
	if ( vorbis_synthesis( &m_pVorbis->block, &m_pVorbis->op ) != 0
		 || vorbis_synthesis_blockin( &m_pVorbis->dspState, &m_pVorbis->block ) != 0 )
		return false;

	int written = 0;
	const int maxSamples = m_nSamples;
	for ( ;; )
	{
		float** pcm = nullptr;
		int available = vorbis_synthesis_pcmout( &m_pVorbis->dspState, &pcm );
		if ( available == 0 )
			break;
		int toConvert = available;
		if ( toConvert > maxSamples )
			toConvert = maxSamples;
		const int channelCount = m_nChannels;
		if ( channelCount > 0 )
		{
			for ( int channel = 0; channel < channelCount; ++channel )
			{
				float* samples = pcm[channel];
				if ( toConvert <= 0 )
					continue;
				float* end = samples + toConvert;
				int offset = channel;
				do
				{
					int sample = static_cast<int>( 32767.0f * *samples );
					if ( sample < -32768 )
						sample = -32768;
					if ( sample > 32767 )
						sample = 32767;
					buffer[written + offset] = static_cast<short>( sample );
					++samples;
					offset += channelCount;
				} while ( samples != end );
			}
		}
		written += toConvert;
		vorbis_synthesis_read( &m_pVorbis->dspState, toConvert );
	}
	numOutSamples = written;
	return true;
}

bool AudioVorbisDecoder::OpenVorbis( const WebMDemuxer& demuxer )
{
	unsigned int privateSize = 0;
	const unsigned char* codecPrivate = demuxer.GetAudioExtradata( privateSize );
	if ( privateSize <= 2 || codecPrivate == nullptr || codecPrivate[0] != 2 )
		return false;

	ogg_packet packets[3];
	if ( !ReadXiphHeaders( codecPrivate, privateSize, packets ) )
		return false;

	VorbisDecoder* vorbis = new VorbisDecoder;
	std::memset( vorbis, 0, sizeof( VorbisDecoder ) );
	m_pVorbis = vorbis;
	vorbis_info_init( &vorbis->info );
	vorbis_comment comment;
	vorbis_comment_init( &comment );
	for ( int packetIndex = 0; packetIndex < 3; ++packetIndex )
	{
		if ( vorbis_synthesis_headerin( &vorbis->info, &comment, &packets[packetIndex] ) != 0 )
		{
			vorbis_comment_clear( &comment );
			return false;
		}
	}
	vorbis_comment_clear( &comment );
	if ( vorbis_synthesis_init( &vorbis->dspState, &vorbis->info ) != 0 )
		return false;
	vorbis->hasDSPState = true;
	if ( vorbis->info.channels != m_nChannels )
		return false;
	const double sampleRate = static_cast<double>( vorbis->info.rate );
	if ( sampleRate == demuxer.GetSampleRate() && vorbis_block_init( &vorbis->dspState, &vorbis->block ) == 0 )
	{
		vorbis->hasBlock = true;
		std::memset( &vorbis->op, 0, sizeof( vorbis->op ) );
		m_nSamples = kVorbisScratchSamples / m_nChannels;
		return true;
	}
	return false;
}

void AudioVorbisDecoder::Close()
{
	ReleaseVorbis( m_pVorbis );
	m_pVorbis = nullptr;
}

static void StartInterfaceSound( const char* name, bool reportMissing )
{
	sfx_t* sound = S_PrecacheSound( const_cast<char*>( name ) );
	if ( sound != nullptr )
	{
		vec3_t origin = { 0.0f, 0.0f, 0.0f };
		S_StartDynamicSound( 0, kPlaybackChannel, sound, origin, 1.0f, 1.0f, 0, kPlaybackPitch );
		return;
	}
	if ( reportMissing )
		Con_DPrintf( "invalid sound %s\n", name );
}

static bool PlaybackKey( const SDL_Event& event )
{
	if ( event.type == SDL_QUIT )
		return true;
	if ( event.type != SDL_KEYDOWN )
		return false;
	const SDL_Keycode key = event.key.keysym.sym;
	return key == SDLK_RETURN || key == SDLK_ESCAPE || key == SDLK_SPACE;
}

void WebMPlayer::PlayVideo( const char* szPath, SDL_Window* pSDLWindow )
{
	Con_Printf( "WebMPlayer::PlayVideo %s\n", szPath );
	mkvparser::MkvReader reader;
	if ( reader.Open( szPath ) != 0 )
	{
		Con_Printf( "failed to find video file %s\n", szPath );
		return;
	}

	WebMDemuxer demuxer( &reader, 0, 0 );
	if ( !demuxer.m_bIsOpen || demuxer.GetWidth() <= 0 || demuxer.GetHeight() <= 0 )
		return;

	StartInterfaceSound( "UI/valve_sound.wav", true );

	int nWindowW = 0;
	int nWindowH = 0;
	SDL_GetWindowSize( pSDLWindow, &nWindowW, &nWindowH );
#ifdef SW_RENDERER
	SDL_Renderer* renderer = SDL_GetRenderer( pSDLWindow );
	bool createdRenderer = false;
	if ( !renderer )
	{
		renderer = SDL_CreateRenderer( pSDLWindow, -1, 0 );
		createdRenderer = true;
	}
	const char* previousScale = SDL_GetHint( SDL_HINT_RENDER_SCALE_QUALITY );
	SDL_SetHint( SDL_HINT_RENDER_SCALE_QUALITY, "2" );
	SDL_Texture* texture = SDL_CreateTexture( renderer, SDL_PIXELFORMAT_BGR888, SDL_TEXTUREACCESS_TARGET,
		demuxer.GetWidth(), demuxer.GetHeight() );
#else
	GLuint texture = 0;
	glGenTextures( 1, &texture );
	glEnable( GL_TEXTURE_RECTANGLE );
	glBindTexture( GL_TEXTURE_RECTANGLE, texture );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	glTexParameteri( GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glDisable( GL_BLEND );
	glDisable( GL_LIGHTING );
	glDisable( GL_DEPTH_TEST );
	glDisable( GL_ALPHA_TEST );
	glDisable( GL_CULL_FACE );
	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();
	glOrtho( 0.0, static_cast<double>( nWindowW ), static_cast<double>( nWindowH ), 0.0, -1.0, 1.0 );
	glMatrixMode( GL_MODELVIEW );
	glPushMatrix();
	glLoadIdentity();
	glViewport( 0, 0, nWindowW, nWindowH );
	glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
#endif

	const int sourceWidth = demuxer.GetWidth();
	const int sourceHeight = demuxer.GetHeight();
	int nVideoW = sourceWidth;
	int nVideoH = sourceHeight;
#if defined( _WIN32 )
	int centerSpan = nWindowH;
	float aspect = (float)sourceWidth / (float)sourceHeight;
	bool scaledWidth = false;
	if ( nVideoW > nWindowW )
	{
		nVideoW = nWindowW;
		scaledWidth = true;
		nVideoH = (int)( (float)nWindowW / aspect );
	}
	if ( nVideoH > nWindowH || ( !scaledWidth && nWindowW > nVideoW ) )
	{
		nVideoH = nWindowH;
		nVideoW = (int)( (float)nWindowH * aspect );
	}
	else if ( !scaledWidth && nWindowH > nVideoH )
	{
		nVideoW = nWindowW;
		nVideoH = (int)( (float)nWindowW / aspect );
	}
#else
	const double widthOverHeight = static_cast<double>( sourceWidth ) / static_cast<double>( sourceHeight );
	int centerSpan = nWindowH;
	if ( nWindowW >= sourceWidth )
	{
		if ( nWindowH < sourceHeight )
		{
			nVideoH = nWindowH;
			nVideoW = TruncateFloat( widthOverHeight * static_cast<double>( nVideoH ) );
			centerSpan = nWindowH;
		}
		else if ( nWindowW > sourceWidth )
		{
			nVideoH = nWindowH;
			nVideoW = TruncateFloat( widthOverHeight * static_cast<double>( nWindowH ) );
			centerSpan = nWindowH;
		}
		else if ( nWindowH > sourceHeight )
		{
			nVideoW = nWindowW;
			nVideoH = TruncateFloat( static_cast<double>( nWindowW ) / widthOverHeight );
			centerSpan = nWindowH;
		}
	}
	else
	{
		nVideoH = nWindowH;
		const int fitted = TruncateFloat( static_cast<double>( nWindowW ) / widthOverHeight );
		if ( fitted > nWindowH )
			nVideoW = TruncateFloat( widthOverHeight * static_cast<double>( nVideoH ) );
		else
		{
			nVideoH = fitted;
			nVideoW = nWindowW;
		}
		centerSpan = nWindowH;
	}
#endif
	const int dstX = nWindowW / 2 - nVideoW / 2;
	const int dstY = centerSpan / 2 - nVideoH / 2;
#ifdef SW_RENDERER
	SDL_Rect destination = { dstX, dstY, nVideoW, nVideoH };
	SDL_SetRenderDrawColor( renderer, 0, 0, 0, 0 );
	SDL_RenderClear( renderer );
	SDL_RenderPresent( renderer );
#endif

	VPXDecoder videoDec( demuxer, kVpxMaxThreads );
	AudioVorbisDecoder audioDec( demuxer );
	WebMFrame videoFrame;
	WebMFrame audioFrame;
	short* pcm = nullptr;
	if ( audioDec.IsOpen() )
	{
		const unsigned int sampleCount =
			static_cast<unsigned int>( audioDec.m_nSamples ) * static_cast<unsigned int>( demuxer.GetChannels() );
#if defined( _WIN32 )
		pcm = new short[sampleCount];
#else
		if ( sampleCount > kPcmCountLimit )
			throw std::bad_array_new_length();
		pcm = static_cast<short*>( ::operator new[]( sampleCount * 2 ) );
#endif
	}

	const int pixelWidth = demuxer.GetWidth();
	const int pixelHeight = demuxer.GetHeight();
	unsigned char* pOutFrame =
		static_cast<unsigned char*>( ::operator new[]( static_cast<unsigned int>( pixelWidth * pixelHeight ) << 2 ) );
	Con_Printf( "Length: %f\n", demuxer.GetLength() );

	const double flStartTime = Sys_FloatTime();
	if ( !demuxer.ReadFrame( &videoFrame, &audioFrame ) )
		goto playback_done;

	{
		double elapsed = 0.0;
		double frameDelay = 0.0;
		double flVideoPos = 0.0;
#ifndef SW_RENDERER
		bool bFirstFrame = true;
#endif
		for ( ;; )
		{
			if ( elapsed + frameDelay >= videoFrame.m_flTime )
			{
				if ( videoDec.m_pCtx != nullptr && videoFrame.m_uBufferSize > 0 )
				{
					if ( !videoDec.Decode( videoFrame ) )
					{
						Con_Printf( "Video decode error\n" );
						goto playback_done;
					}
					for ( ;; )
					{
						vpx_image_t* image = vpx_codec_get_frame( videoDec.m_pCtx, &videoDec.m_pIter );
						if ( image == nullptr )
							break;
						if ( image->cs != 0 )
							videoDec.m_nLastSpace = image->cs;
						if ( ( static_cast<unsigned int>( image->fmt ) & kVpxFmtAcceptedMask ) != VPX_IMG_FMT_PLANAR )
							break;
						if ( image->stride[0] == 0 || image->stride[1] == 0 || image->stride[2] == 0 )
							break;

						const bool uvFlip = ( static_cast<unsigned int>( image->fmt ) & VPX_IMG_FMT_UV_FLIP ) != 0;
						const int imageW = static_cast<int>( image->d_w );
						const int imageH = static_cast<int>( image->d_h );
						const int ySpan = image->stride[0];
						const int uvSpan = uvFlip ? image->stride[2] : image->stride[1];
						const unsigned char* yPlane = image->planes[0];
						const unsigned char* uPlane = uvFlip ? image->planes[1] : image->planes[2];
						const unsigned char* vPlane = uvFlip ? image->planes[2] : image->planes[1];
						const unsigned int xShift = image->x_chroma_shift;
						const unsigned int yShift = image->y_chroma_shift;
						if ( xShift == 1 && yShift == 1 )
						{
							yuv420_2_rgb8888( pOutFrame, yPlane, uPlane, vPlane, imageW, imageH, ySpan, uvSpan,
								4 * imageW, yuv2rgb565_table, 0 );
						}
						else if ( xShift == 1 && yShift == 0 )
						{
							yuv422_2_rgb8888( pOutFrame, yPlane, uPlane, vPlane, imageW, imageH, ySpan, uvSpan,
								4 * imageW, yuv2rgb565_table, 0 );
						}
						else if ( xShift == 0 && yShift == 0 )
						{
							yuv444_2_rgb8888( pOutFrame, yPlane, uPlane, vPlane, imageW, imageH, ySpan, uvSpan,
								4 * imageW, yuv2rgb565_table, 0 );
						}
						else
						{
							continue;
						}

#ifdef SW_RENDERER
						SDL_UpdateTexture( texture, NULL, pOutFrame, 4 * imageW );
						SDL_RenderClear( renderer );
						SDL_RenderCopy( renderer, texture, NULL, &destination );
						SDL_RenderPresent( renderer );
#else
						if ( bFirstFrame )
						{
							glTexImage2D( GL_TEXTURE_RECTANGLE, 0, GL_RGBA, imageW, imageH, 0, GL_RGBA,
								GL_UNSIGNED_BYTE, pOutFrame );
						}
						else
						{
							glTexSubImage2D(
								GL_TEXTURE_RECTANGLE, 0, 0, 0, imageW, imageH, GL_RGBA, GL_UNSIGNED_BYTE, pOutFrame );
						}
						glClear( GL_COLOR_BUFFER_BIT );
						glBegin( GL_QUADS );
						const float left = static_cast<float>( dstX );
						const float top = static_cast<float>( dstY );
						const float right = static_cast<float>( dstX + nVideoW );
						const float bottom = static_cast<float>( dstY + nVideoH );
						glTexCoord2i( 0, 0 );
						glVertex3f( left, top, 0.0f );
						glTexCoord2i( imageW, 0 );
						glVertex3f( right, top, 0.0f );
						glTexCoord2i( imageW, imageH );
						glVertex3f( right, bottom, 0.0f );
						glTexCoord2i( 0, imageH );
						glVertex3f( left, bottom, 0.0f );
						glEnd();
						bFirstFrame = false;
						SDL_GL_SwapWindow( pSDLWindow );
#endif
					}
				}

				if ( audioDec.IsOpen() && audioFrame.m_uBufferSize > 0 )
				{
					int sampleCount = 0;
					if ( !audioDec.GetPCMS16( audioFrame, pcm, sampleCount ) )
					{
						Con_Printf( "Audio decode error\n" );
						goto playback_done;
					}
				}
				const double presented = videoFrame.m_flTime;
				if ( !demuxer.ReadFrame( &videoFrame, &audioFrame ) )
					goto playback_done;
				if ( videoFrame.m_flTime >= demuxer.GetLength() )
					goto playback_done;
				frameDelay = presented - flVideoPos;
				flVideoPos = presented;
			}

			S_Update( vec3_origin, vec3_origin, vec3_origin, vec3_origin );
			bool skipped = false;
			SDL_Event event;
			while ( SDL_PollEvent( &event ) != 0 )
			{
				if ( PlaybackKey( event ) )
					skipped = true;
			}
			SDL_PumpEvents();
			Sys_Sleep( 1 );
			if ( skipped )
			{
				Con_Printf( "VIDEO PLAYBACK: user skipped playback.\n" );
				StartInterfaceSound( "common/null.wav", false );
				goto playback_done;
			}
			elapsed = Sys_FloatTime() - flStartTime;
		}
	}

playback_done:
#ifdef SW_RENDERER
	SDL_SetHint( SDL_HINT_RENDER_SCALE_QUALITY, previousScale );
	SDL_DestroyTexture( texture );
	if ( createdRenderer )
		SDL_DestroyRenderer( renderer );
#else
	glBindTexture( GL_TEXTURE_RECTANGLE, 0 );
	glDisable( GL_TEXTURE_RECTANGLE );
	glDeleteTextures( 1, &texture );
#endif
	::operator delete[]( pOutFrame );
	::operator delete[]( pcm );
}
