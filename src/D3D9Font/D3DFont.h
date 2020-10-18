//
// Added D3DX Slerp vector helper functions and old deprecated D3D9 structs because the latest DirectX SDK 
// doesn't include those anymore (Microsoft's d3d9tex lib is no longer available in Win10 VisualStudio environment without doing all sort of tricks).
// This header and class implements the bare minimum classes and structs to create D3D9 compatible fonts and D3D9 matrix transformations used in RBR plugin.
//

#ifndef D3DFONT_H
#define D3DFONT_H

#include <tchar.h>
#include "SharedFont.h"

#include <vector>
#include <memory>

// Font creation flags
#define D3DFONT_BOLD        0x0001
#define D3DFONT_ITALIC      0x0002
#define D3DFONT_ZENABLE     0x0004

// Font rendering flags
#define D3DFONT_CENTERED_X  0x0001
#define D3DFONT_CENTERED_Y  0x0002
#define D3DFONT_TWOSIDED    0x0004
#define D3DFONT_FILTERED    0x0008
#define D3DFONT_BORDER		0x0010
#define D3DFONT_COLORTABLE	0x0020

#define D3DFONT_CLEARTARGET 0x0080

// Defined deprecated D3Dx9.h struct because the old DX9 derived code uses these structs
typedef struct D3DXVECTOR4 {
	FLOAT x;
	FLOAT y;
	FLOAT z;
	FLOAT w;

	D3DXVECTOR4() { x = y = z = w = 0; }

	D3DXVECTOR4(float initX, float initY, float initZ, float initW)
	{
		x = initX; y = initY; z = initZ; w = initW;
	}
} D3DXVECTOR4, *LPD3DXVECTOR4;

// Defined deprecated D3Dx9.h struct
typedef struct D3DXVECTOR3 {
#pragma pack(push,1)
	FLOAT x;
	FLOAT y;
	FLOAT z;
#pragma pack(pop)

	//D3DXVECTOR3() { x = y = z = 0; }

} D3DXVECTOR3, *LPD3DXVECTOR3;


// Defined deprecated D3Dx9math.h struct
typedef struct D3DXQUATERNION {
#pragma pack(push,1)
	FLOAT x;
	FLOAT y;
	FLOAT z;
	FLOAT w;
#pragma pack(pop)
} D3DXQUATERNION, *LPD3DXQUATERNION;

// Defined deprecated D3Dx9math.h D3DXVec3Lerp implementation
inline D3DXVECTOR3* D3DXVec3Lerp(D3DXVECTOR3 *pout, CONST D3DXVECTOR3 *pv1, CONST D3DXVECTOR3 *pv2, FLOAT s)
{
	// Wine version
	//if (!pout || !pv1 || !pv2) return NULL;
	//pout->x = (1 - s) * (pv1->x) + s * (pv2->x);
	//pout->y = (1 - s) * (pv1->y) + s * (pv2->y);
	//pout->z = (1 - s) * (pv1->z) + s * (pv2->z);
	//return pout;

	pout->x = pv1->x + s * (pv2->x - pv1->x);
	pout->y = pv1->y + s * (pv2->y - pv1->y);
	pout->z = pv1->z + s * (pv2->z - pv1->z);
	return pout;
}

// Defined deprecated D3Dx9math function
inline float D3DXQuaternionDot(const D3DXQUATERNION *pQ1, const D3DXQUATERNION *pQ2)
{
	return (pQ1->x * pQ2->x + pQ1->y * pQ2->y + pQ1->z * pQ2->z + pQ1->w * pQ2->w);
}

// Defined deprecated D3Dx9math function. Borrowed from WineHQ implementation.
inline FLOAT D3DXQuaternionLength(CONST D3DXQUATERNION *pq)
{
	return sqrtf((pq->x) * (pq->x) + (pq->y) * (pq->y) + (pq->z) * (pq->z) + (pq->w) * (pq->w));
}

inline D3DXQUATERNION* D3DXQuaternionNormalize(D3DXQUATERNION *pOut, CONST D3DXQUATERNION *pq)
{
	FLOAT norm = D3DXQuaternionLength(pq);
	if (!norm)
		// Zero length, so let's just return zero quat instead of running into "divided by zero" error
		pOut->x = pOut->y = pOut->z = pOut->w = 0.0f;
	else
	{
		pOut->x = pq->x / norm;
		pOut->y = pq->y / norm;
		pOut->z = pq->z / norm;
		pOut->w = pq->w / norm;
	}
	return pOut;
}


inline D3DXQUATERNION* D3DXQuaternionNormalize(D3DXQUATERNION *pInOut)
{
	FLOAT norm = D3DXQuaternionLength(pInOut);
	if (!norm)
		pInOut->x = pInOut->y = pInOut->z = pInOut->w = 0.0f;
	else
	{
		pInOut->x /= norm;
		pInOut->y /= norm;
		pInOut->z /= norm;
		pInOut->w /= norm;
	}
	return pInOut;
}

inline D3DXQUATERNION* D3DXQuaternionLerp(D3DXQUATERNION *pOut, const D3DXQUATERNION *pQ1, const D3DXQUATERNION *pQ2, float t)
{
	//return (q1*(1 - t) + q2*t).normalized();
	//float t1 = 1.0f - t;
	//pOut->x = pQ1->x * t1 + pQ2->x * t;
	//pOut->y = pQ1->y * t1 + pQ2->y * t;
	//pOut->z = pQ1->z * t1 + pQ2->z * t;
	//pOut->w = pQ1->w * t1 + pQ2->w * t;

	pOut->x = pQ1->x + t * (pQ2->x - pQ1->x);
	pOut->y = pQ1->y + t * (pQ2->y - pQ1->y);
	pOut->z = pQ1->z + t * (pQ2->z - pQ1->z);
	pOut->w = pQ1->w + t * (pQ2->w - pQ1->w);
	return D3DXQuaternionNormalize(pOut);
}

inline D3DXQUATERNION* D3DXQuaternionSlerp(D3DXQUATERNION *pOut, const D3DXQUATERNION *pQ1, const D3DXQUATERNION *pQ2, float t)
{
/*	
	// ReactOS. Works OK.
    FLOAT dot, temp;
	temp = 1.0f - t;
	dot = D3DXQuaternionDot(pQ1, pQ2);
	if (dot < 0.0f)
	{
		t = -t;
		dot = -dot;
	}
	if (1.0f - dot > 0.001f)
	{
		FLOAT theta = acosf(dot);
		temp = sinf(theta * temp) / sinf(theta);
		t = sinf(theta * t) / sinf(theta);
	}
	pOut->x = temp * pQ1->x + t * pQ2->x;
	pOut->y = temp * pQ1->y + t * pQ2->y;
	pOut->z = temp * pQ1->z + t * pQ2->z;
	pOut->w = temp * pQ1->w + t * pQ2->w;
	return pOut;
*/

	// http://willperone.net/Code/quaternion.php
	// if (dot < 0), q1 and q2 are more than 90 degrees apart, so we can invert one to reduce spinning

	D3DXQUATERNION pQ3;
	float dot = D3DXQuaternionDot(pQ1, pQ2);
	if (dot < 0.0f)
	{
		// Take the shorter "spinning" route to the pQ2 target orientation by reversing the orientation (otherwise Slerp would travel the longer "circle path" to the target)
		dot = -dot;
		//pQ3 = -pQ2;
		pQ3.x = -pQ2->x;
		pQ3.y = -pQ2->y;
		pQ3.z = -pQ2->z;
		pQ3.w = -pQ2->w;
	}
	else
	{
		//pQ3 = pQ2;
		pQ3.x = pQ2->x;
		pQ3.y = pQ2->y;
		pQ3.z = pQ2->z;
		pQ3.w = pQ2->w;
	}

	if (dot < 0.95f)
	{
		float angle = acosf(dot);
		//return (q1*sinf(angle*(1 - t)) + q3*sinf(angle*t)) / sinf(angle);
	
		float angle_sinf1 = sinf(angle * (1.0f - t));
		float angle_sinft = sinf(angle * t);
		float angle_sinf  = sinf(angle);

		pOut->x = (pQ1->x * angle_sinf1 + pQ3.x * angle_sinft) / angle_sinf;
		pOut->y = (pQ1->y * angle_sinf1 + pQ3.y * angle_sinft) / angle_sinf;
		pOut->z = (pQ1->z * angle_sinf1 + pQ3.z * angle_sinft) / angle_sinf;
		pOut->w = (pQ1->w * angle_sinf1 + pQ3.w * angle_sinft) / angle_sinf;
	}
	else 
	{
		// The travel distance is very short, so do normalized linear interpolation instead of spherical linear interpolation (the end result is pretty much the same in short steps but Lerp is a lot faster operation than SLerp)
		D3DXQuaternionLerp(pOut, pQ1, &pQ3, t);
	}
	return pOut;

/*
	// WineHQ version
	FLOAT dot, epsilon;
	epsilon = 1.0f;
	dot = D3DXQuaternionDot(pQ1, pQ2);
	if (dot < 0.0f) epsilon = -1.0f;
	pOut->x = (1.0f - t) * pQ1->x + epsilon * t * pQ2->x;
	pOut->y = (1.0f - t) * pQ1->y + epsilon * t * pQ2->y;
	pOut->z = (1.0f - t) * pQ1->z + epsilon * t * pQ2->z;
	pOut->w = (1.0f - t) * pQ1->w + epsilon * t * pQ2->w;
	return pOut;
*/
/*	// Other alternative implementation
    float theta = acosf(D3DXQuaternionDot(pQ1, pQ2));
	float sinTheta = sinf(theta);
	float t1 = sinf((1.0f - t) * theta) / sinTheta;
	float t2 = sinf(t * theta) / sinTheta;

	pOut->x = pQ1->x * t1 + pQ2->x * t2;
	pOut->y = pQ1->y * t1 + pQ2->y * t2;
	pOut->z = pQ1->z * t1 + pQ2->z * t2;
	pOut->w = pQ1->w * t1 + pQ2->w * t2;
	return pOut;
*/
}

// Implement deprecated DX9 D3DXMatrixIdentity function because the D3dx9.lib is no longer available in newer DirectX SDK libraries. DX12 has new libraries, but RBR is DX9 game.
// Specs https://docs.microsoft.com/en-us/windows/win32/direct3d9/d3dxmatrixidentity
inline D3DMATRIX* D3DXMatrixIdentity(D3DMATRIX* pout)
{
	//pout->m[0][1] = pout->m[0][2] = pout->m[0][3] = pout->m[1][0] = pout->m[1][2] = 
	//pout->m[1][3] = pout->m[2][0] = pout->m[2][1] = pout->m[2][3] = pout->m[3][0] = 
	//pout->m[3][1] = pout->m[3][2] = 0.0f;
	ZeroMemory(pout, sizeof(D3DMATRIX));	
	pout->m[0][0] = pout->m[1][1] = pout->m[2][2] = pout->m[3][3] = 1.0f;

	return pout;
}

// Implement deprecated DX9 D3DXMatrixRotationQuaternion function because the D3dx9.lib is no longer available in newer DirectX SDK libraries. DX12 has new libraries, but RBR is DX9 game.
// Borrowed from ReactOS sources. https://doxygen.reactos.org/
inline D3DMATRIX* D3DXMatrixRotationQuaternion(D3DMATRIX* pout, const D3DXQUATERNION* pq)
{
	D3DXMatrixIdentity(pout);
	pout->m[0][0] = 1.0f - 2.0f * (pq->y * pq->y + pq->z * pq->z);
	pout->m[0][1] = 2.0f * (pq->x * pq->y + pq->z * pq->w);
	pout->m[0][2] = 2.0f * (pq->x * pq->z - pq->y * pq->w);
	pout->m[1][0] = 2.0f * (pq->x * pq->y - pq->z * pq->w);
	pout->m[1][1] = 1.0f - 2.0f * (pq->x * pq->x + pq->z * pq->z);
	pout->m[1][2] = 2.0f * (pq->y * pq->z + pq->x * pq->w);
	pout->m[2][0] = 2.0f * (pq->x * pq->z + pq->y * pq->w);
	pout->m[2][1] = 2.0f * (pq->y * pq->z - pq->x * pq->w);
	pout->m[2][2] = 1.0f - 2.0f * (pq->x * pq->x + pq->y * pq->y);
	return pout;
}


//-----------------------------------------------------------------------------
// Name: class CD3DFont
// Desc: Texture-based font class for doing text in a 3D scene.
//-----------------------------------------------------------------------------

extern BOOL g_bD3DFontReleaseStateBlocks; // TRUE - Destructor releases stateBlock objects. FALSE - Destructor does not call release for stateBlocks because it has been done already

class CD3DFont
{
	static std::vector<std::shared_ptr<SharedFont>> sharedFonts;

	static std::shared_ptr<SharedFont> GetFont(const std::wstring &fontName, DWORD dwHeight, DWORD dwFlags);
	static void ReleaseFont(std::shared_ptr<SharedFont> font);

	LPDIRECT3DDEVICE9       m_pd3dDevice; // A D3DDevice used for rendering
	LPDIRECT3DVERTEXBUFFER9 m_pVB;        // VertexBuffer for rendering text

	// Stateblocks for setting and restoring render states
	LPDIRECT3DSTATEBLOCK9 m_pStateBlockSaved;
	LPDIRECT3DSTATEBLOCK9 m_pStateBlockDrawText;

	std::shared_ptr<SharedFont> m_font;
	DWORD m_dwFlags;

	int m_iTextHeight;  // Actual text height in pixels (cached value after the first call to GetTextHeight)

public:
	BOOL IsFontIdentical(const std::wstring& fontName, DWORD dwHeight, DWORD dwFlags); // TRUE/FALSE if this CD3DFont represents identical font style

	// 2D and 3D text drawing functions
	//HRESULT DrawText(FLOAT x, FLOAT y, DWORD dwColor, const WCHAR* strText, DWORD dwFlags = 0L);
	HRESULT DrawText(int x, int y, DWORD dwColor, const CHAR*  strText, DWORD dwFlags = 0L);
	HRESULT DrawText(int x, int y, DWORD dwColor, const WCHAR* strText, DWORD dwFlags = 0L);

	// Function to get extent of text
	HRESULT GetTextExtent(const WCHAR* strText, SIZE* pSize);
	int GetTextHeight();

	// Initializing and destroying device-dependent objects
	HRESULT InitDeviceObjects(LPDIRECT3DDEVICE9 pd3dDevice);
	HRESULT RestoreDeviceObjects();
	HRESULT InvalidateDeviceObjects();
	HRESULT DeleteDeviceObjects();

	LPDIRECT3DDEVICE9 getDevice() const { return m_pd3dDevice; }

	// Constructor / destructor
	CD3DFont(const std::wstring &fontName, DWORD dwHeight, DWORD dwFlags = 0L);
	~CD3DFont();
};

#endif
