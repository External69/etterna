#include "stdafx.h"
/*
-----------------------------------------------------------------------------
 File: Sprite.cpp

 Desc: A bitmap actor that animates and moves around.

 Copyright (c) 2001 Chris Danford.  All rights reserved.
-----------------------------------------------------------------------------
*/


#include "Sprite.h"
#include "RageTextureManager.h"
#include "IniFile.h"
#include <assert.h>
#include <math.h>


Sprite::Sprite()
{
	Init();
}

void Sprite::Init()
{
	Actor::Init();

	m_pTexture = NULL;
	m_uNumStates = 0;
	m_uCurState = 0;
	m_bIsAnimating = TRUE;
	m_fSecsIntoState = 0.0;
	m_bUsingCustomTexCoordRect = FALSE ;
	m_Effect =  no_effect ;
	m_fPercentBetweenColors = 0.0f ;
	m_bTweeningTowardEndColor = TRUE ;
	m_fDeltaPercentPerSecond = 1.0 ;
	m_fWagRadians =  0.2f ;
	m_fWagPeriod =  2.0f ;
	m_fWagTimer =  0.0f ;
	m_fSpinSpeed =  2.0f ;
	m_fVibrationDistance =  5.0f ;
	m_bVisibleThisFrame =  FALSE;
}

Sprite::~Sprite()
{
//	RageLog( "Sprite Destructor" );

	TM->UnloadTexture( m_sTexturePath ); 
}


bool Sprite::LoadFromTexture( CString sTexturePath )
{
	RageLog( ssprintf("Sprite::LoadFromTexture(%s)", sTexturePath) );

	Init();
	return LoadTexture( sTexturePath );
}

// Sprite file has the format:
//
// [Sprite]
// Texture=Textures\Logo 1x1.bmp
// Frame0000=0
// Delay0000=1.0
// Frame0001=3
// Delay0000=2.0
bool Sprite::LoadFromSpriteFile( CString sSpritePath )
{
	RageLog( ssprintf("Sprite::LoadFromSpriteFile(%s)", sSpritePath) );

	Init();

	m_sSpritePath = sSpritePath;

	// read sprite file
	IniFile ini;
	ini.SetPath( m_sSpritePath );
	if( !ini.ReadFile() )
		RageError( ssprintf("Error opening Sprite file '%s'.", m_sSpritePath) );

	CString sTexturePath = ini.GetValue( "Sprite", "Texture" );
	if( sTexturePath == "" )
		RageError( ssprintf("Error reading  value 'Texture' from %s.", m_sSpritePath) );

	// Load the texture
	if( !LoadTexture( sTexturePath ) )
		return FALSE;


	// Read in frames and delays from the sprite file, 
	// overwriting the states that LoadFromTexture created.
	for( UINT i=0; i<MAX_SPRITE_STATES; i++ )
	{
		CString sStateNo;
		sStateNo.Format( "%u%u%u%u", (i%10000)/1000, (i%1000)/100, (i%100)/10, (i%10) );	// four digit state no

		CString sFrameKey( CString("Frame") + sStateNo );
		CString sDelayKey( CString("Delay") + sStateNo );
		
		m_uFrame[i] = ini.GetValueI( "Sprite", sFrameKey );
		if( m_uFrame[i] >= m_pTexture->GetNumFrames() )
			RageError( ssprintf("In '%s', %s is %d, but the texture %s only has %d frames.",
						m_sSpritePath, sFrameKey, m_uFrame[i], sTexturePath, m_pTexture->GetNumFrames()) );
		m_fDelay[i] = (float)ini.GetValueF( "Sprite", sDelayKey );

		if( m_uFrame[i] == 0  &&  m_fDelay[i] > -0.00001f  &&  m_fDelay[i] < 0.00001f )	// both values are empty
			break;

		m_uNumStates = i+1;
	}

	if( m_uNumStates == 0 )
		RageError( ssprintf("Failed to find at least one state in %s.", m_sSpritePath) );

	return TRUE;
}

bool Sprite::LoadTexture( CString sTexturePath )
{
	if( m_sTexturePath != "" )			// If there was a previous bitmap...
		TM->UnloadTexture( m_sTexturePath );	// Unload it.


	m_sTexturePath = sTexturePath;

	m_pTexture = TM->LoadTexture( m_sTexturePath );
	assert( m_pTexture != NULL );

	// the size of the sprite is the size of the image before it was scaled
	SetWidth( (float)m_pTexture->GetSourceFrameWidth() );
	SetHeight( (float)m_pTexture->GetSourceFrameHeight() );		

	// Assume the frames of this animation play in sequential order with 0.2 second delay.
	for( UINT i=0; i<m_pTexture->GetNumFrames(); i++ )
	{
		m_uFrame[i] = i;
		m_fDelay[i] = 0.1f;
		m_uNumStates = i+1;
	}
		
	return TRUE;
}


void Sprite::PrintDebugInfo()
{
//	Actor::PrintDebugInfo();

	RageLog( "Sprite::PrintDebugInfo()" );
	RageLog( "m_uNumStates: %u, m_uCurState: %u, m_fSecsIntoState: %f", 
		      m_uNumStates, m_uCurState, m_fSecsIntoState );
}


void Sprite::Update( const float &fDeltaTime )
{
	//PrintDebugInfo();

	Actor::Update( fDeltaTime );	// do tweening


	// update animation
	if( m_bIsAnimating )
	{
		m_fSecsIntoState += fDeltaTime;

		if( m_fSecsIntoState > m_fDelay[m_uCurState] )		// it's time to switch frames
		{
			// increment frame and reset the counter
			m_fSecsIntoState -= m_fDelay[m_uCurState];		// leave the left over time for the next frame
			m_uCurState ++;
			if( m_uCurState >= m_uNumStates )
				m_uCurState = 0;
		}
	}




}


void Sprite::Draw()
{
	Actor::Draw();	// set up our world matrix

	if( m_pTexture == NULL )
		return;
	
	FRECT* pTexCoordRect;	// the texture coordinates of the frame we're going to use
	if( m_bUsingCustomTexCoordRect ) {
		pTexCoordRect = &m_CustomTexCoordRect;
	} else {
		UINT uFrameNo = m_uFrame[m_uCurState];
		pTexCoordRect = m_pTexture->GetTextureCoordRect( uFrameNo );
	}


	D3DXCOLOR	colorDiffuse	= m_colorDiffuse;
	D3DXCOLOR	colorAdd		= m_colorAdd;

	// update properties based on SpriteEffects 
	switch( m_Effect )
	{
	case no_effect:
		break;
	case blinking:
		colorDiffuse = m_bTweeningTowardEndColor ? m_start_colorDiffuse : m_end_colorDiffuse;
		break;
	case camelion:
		colorDiffuse = m_start_colorDiffuse*m_fPercentBetweenColors + m_end_colorDiffuse*(1.0f-m_fPercentBetweenColors);
		break;
	case glowing:
		colorAdd = m_start_colorAdd*m_fPercentBetweenColors + m_end_colorAdd*(1.0f-m_fPercentBetweenColors);
		break;
	case wagging:
		break;
	case spinning:
		// nothing special needed
		break;
	case vibrating:
		break;
	case flickering:
		m_bVisibleThisFrame = !m_bVisibleThisFrame;
		if( !m_bVisibleThisFrame )
			colorDiffuse = D3DXCOLOR(0,0,0,0);		// don't draw the frame
		break;
	}


	// make the object in logical units centered at the origin
	LPDIRECT3DVERTEXBUFFER8 pVB = SCREEN->GetVertexBuffer();
	CUSTOMVERTEX* v;
	pVB->Lock( 0, 0, (BYTE**)&v, 0 );

	float fHalfSizeX = m_size.x/2;
	float fHalfSizeY = m_size.y/2;

	v[0].p = D3DXVECTOR3( -fHalfSizeX,	 fHalfSizeY,	0 );	// bottom left
	v[1].p = D3DXVECTOR3( -fHalfSizeX,	-fHalfSizeY,	0 );	// top left
	v[2].p = D3DXVECTOR3(  fHalfSizeX,	 fHalfSizeY,	0 );	// bottom right
	v[3].p = D3DXVECTOR3(  fHalfSizeX,	-fHalfSizeY,	0 );	// top right

	v[0].tu = pTexCoordRect->left;		v[0].tv = pTexCoordRect->bottom;	// bottom left
	v[1].tu = pTexCoordRect->left;		v[1].tv = pTexCoordRect->top;		// top left
	v[2].tu = pTexCoordRect->right;		v[2].tv = pTexCoordRect->bottom;	// bottom right
	v[3].tu = pTexCoordRect->right;		v[3].tv = pTexCoordRect->top;		// top right

	v[0].color = v[1].color = v[2].color = v[3].color = colorDiffuse;
	
	pVB->Unlock();


	//////////////////////
	// render the diffuse pass
	//////////////////////

	// set texture and alpha properties
	LPDIRECT3DDEVICE8 pd3dDevice = SCREEN->GetDevice();
    pd3dDevice->SetTexture( 0, m_pTexture->GetD3DTexture() );

	pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );//bBlendAdd ? D3DTOP_ADD : D3DTOP_MODULATE );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE );//bBlendAdd ? D3DTOP_ADD : D3DTOP_MODULATE );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
	//pd3dDevice->SetRenderState( D3DRS_SRCBLEND,  bBlendAdd ? D3DBLEND_ONE : D3DBLEND_SRCALPHA );
	//pd3dDevice->SetRenderState( D3DRS_DESTBLEND, bBlendAdd ? D3DBLEND_ONE : D3DBLEND_INVSRCALPHA );
	pd3dDevice->SetRenderState( D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA );
	pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );


	// finally!  Pump those triangles!	
	pd3dDevice->SetVertexShader( D3DFVF_CUSTOMVERTEX );
	pd3dDevice->SetStreamSource( 0, pVB, sizeof(CUSTOMVERTEX) );
	pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2 );


	//////////////////////
	// render the diffuse pass
	//////////////////////
	if( colorAdd.a == 0 )	// no need to render an add pass
 		return;

	pVB->Lock( 0, 0, (BYTE**)&v, 0 );

	v[0].color = v[1].color = v[2].color = v[3].color = colorAdd;
	
	pVB->Unlock();

	pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG2 );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE );
	
	pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2 );

}


void Sprite::SetState( UINT uNewState )
{
	ASSERT( uNewState >= 0  &&  uNewState < m_uNumStates );
	m_uCurState = uNewState;
	m_fSecsIntoState = 0.0; 
}

void Sprite::SetCustomSrcRect(	FRECT new_texcoord_frect ) 
{ 
	m_bUsingCustomTexCoordRect = true;
	m_CustomTexCoordRect = new_texcoord_frect; 
}


