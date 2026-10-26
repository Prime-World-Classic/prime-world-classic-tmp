#pragma once

#include "../System/BitData.h"
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NScene
{

class BitMap
{
  ZDATA
		CArray2D1Bit mask;
		CVec2 mult;
public:
  ZEND int operator&( IBinSaver &f ) { f.Add(2,&mask); f.Add(3,&mult); return 0; }

private:
	int GetX( const float x ) const { return Clamp( (int)floor( x * mult.x ), 0, mask.GetSizeX() ); }
	int GetY( const float y ) const { return Clamp( (int)floor( y * mult.y ), 0, mask.GetSizeY() ); }
	BitMap( const char* data, float mapSizeX, float mapSizeY, int maskSize );

public:
  BitMap() : mult( VNULL2 ) {}

  BitMap( bool water, const CVec2& mapSize )
  {
    mask.SetSizes( 1, 1 );

    if ( water )
      mask.Set1();
    else
      mask.FillZero();

    mult.x = 1.0f/mapSize.x;
    mult.y = 1.0f/mapSize.y;
  }

  bool CheckPos( const CVec2& pos ) const
  {
		NI_VERIFY( !mask.IsEmpty(), "BitMap not initialized", return false );
		return mask.GetData( GetX( pos.x ), GetY( pos.y ) ) > 0;
  }

  // Grow the masked area by `dist` world units in every direction (square brush, two separable sliding-window passes).
  // Used by the world logic copy of the water mask: the painted mask covers the deep part of the river only.
  void DilateWorld( float dist )
  {
    if ( mask.IsEmpty() || dist <= 0.0f )
      return;
    const int r = (int)ceil( dist * Max( mult.x, mult.y ) );
    const int sx = mask.GetSizeX(), sy = mask.GetSizeY();
    CArray2D1Bit rows( sx, sy );
    rows.FillZero();
    for ( int y = 0; y < sy; ++y )
    {
      int count = 0;
      for ( int x = 0; x < r && x < sx; ++x )
        count += mask.GetData( x, y );
      for ( int x = 0; x < sx; ++x )
      {
        if ( x + r < sx ) count += mask.GetData( x + r, y );
        if ( x - r - 1 >= 0 ) count -= mask.GetData( x - r - 1, y );
        if ( count > 0 ) rows.SetData( x, y );
      }
    }
    mask.FillZero();
    for ( int x = 0; x < sx; ++x )
    {
      int count = 0;
      for ( int y = 0; y < r && y < sy; ++y )
        count += rows.GetData( x, y );
      for ( int y = 0; y < sy; ++y )
      {
        if ( y + r < sy ) count += rows.GetData( x, y + r );
        if ( y - r - 1 >= 0 ) count -= rows.GetData( x, y - r - 1 );
        if ( count > 0 ) mask.SetData( x, y );
      }
    }
  }

  // Keep only the connected masked areas that cross the middle column of the map (the river between the bases);
  // the other areas (lakes and ponds of a base) are dropped. Used by the world logic copy of the water mask.
  void KeepAreasCrossingMiddle()
  {
    if ( mask.IsEmpty() )
      return;
    const int sx = mask.GetSizeX(), sy = mask.GetSizeY();
    const int cx = sx / 2;
    CArray2D1Bit keep( sx, sy );
    keep.FillZero();
    vector<int> stack;
    for ( int y0 = 0; y0 < sy; ++y0 )
    {
      if ( !mask.GetData( cx, y0 ) || keep.GetData( cx, y0 ) )
        continue;
      keep.SetData( cx, y0 );
      stack.push_back( y0 * sx + cx );
      while ( !stack.empty() )
      {
        const int i = stack.back();
        stack.pop_back();
        const int x = i % sx, y = i / sx;
        const int nx[4] = { x - 1, x + 1, x, x };
        const int ny[4] = { y, y, y - 1, y + 1 };
        for ( int k = 0; k < 4; ++k )
        {
          if ( nx[k] < 0 || nx[k] >= sx || ny[k] < 0 || ny[k] >= sy )
            continue;
          if ( mask.GetData( nx[k], ny[k] ) && !keep.GetData( nx[k], ny[k] ) )
          {
            keep.SetData( nx[k], ny[k] );
            stack.push_back( ny[k] * sx + nx[k] );
          }
        }
      }
    }
    mask = keep;
  }

	static void Store( const char* data, float mapSizeX, float mapSizeY, int maskSize, const string& fileName );
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
}