#include "ff.h"

#if FF_USE_LFN

WCHAR ff_uni2oem(	/* Returns converted character, zero on error */
	DWORD	uni,	/* Character code to be converted */
	WORD	cp		/* OEM code page */
)
{
	return uni;
}

WCHAR ff_oem2uni(	/* Returns converted character, zero on error */
	WCHAR	oem,	/* Character code to be converted */
	WORD	cp		/* OEM code page */
)
{
	return oem;
}

DWORD ff_wtoupper (	/* Returns upper converted character */
	DWORD uni		/* Unicode character to be upper converted (BMP only) */
)
{
    if (uni >= 'a' && uni <= 'z')
    {
        uni -= 0x20;
    }

	return uni;
}

#endif
