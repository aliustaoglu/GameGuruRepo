//----------------------------------------------------
//--- GAMEGURU - M-Lighting
//----------------------------------------------------

#include "gameguru.h"

//  ***** Included Source File *****
//  TMAPPER
// 

void lighting_refresh ( void )
{

	//  by default, no override
	t.lighting.override=0;

	//  create infinilight list from all active entities
	//t.t_s="";
	g.infinilightmax=0;
	for ( t.tle = 1 ; t.tle<=  g.entityelementlist; t.tle++ )
	{
		//t.t_s += cstr("tle=");
		//t.t_s += cstr(Str(t.tle));
		//t.t_s += cstr(" ");
		t.tlentid=t.entityelement[t.tle].bankindex;
		//t.t_s += cstr("tlentid=");
		//t.t_s += cstr(Str(t.tlentid));
		//t.t_s += cstr(" ");
		if (  t.tlentid>0 ) 
		{
			//t.t_s += cstr("ismarker=");
			//t.t_s += cstr(Str(t.entityprofile[t.tlentid].ismarker));
			//t.t_s += cstr(" ");
			if (  t.entityprofile[t.tlentid].ismarker == 2 ) 
			{
				++g.infinilightmax;
				Dim (  t.infinilight,g.infinilightmax  );
				t.infinilight[g.infinilightmax].used=1;
				t.infinilight[g.infinilightmax].type=t.entityelement[t.tle].staticflag;
				t.infinilight[g.infinilightmax].x=t.entityelement[t.tle].x;
				t.infinilight[g.infinilightmax].y=t.entityelement[t.tle].y;
				t.infinilight[g.infinilightmax].z=t.entityelement[t.tle].z;
				t.infinilight[g.infinilightmax].range=t.entityelement[t.tle].eleprof.light.range;
				t.infinilight[g.infinilightmax].id=0;
				t.infinilight[g.infinilightmax].dist=0;
				t.infinilight[g.infinilightmax].colrgb.r=RgbR(t.entityelement[t.tle].eleprof.light.color);
				t.infinilight[g.infinilightmax].colrgb.g=RgbG(t.entityelement[t.tle].eleprof.light.color);
				t.infinilight[g.infinilightmax].colrgb.b=RgbB(t.entityelement[t.tle].eleprof.light.color);
				t.entityelement[t.tle].eleprof.light.islit=1;
				t.infinilight[g.infinilightmax].islit=t.entityelement[t.tle].eleprof.light.islit;
				t.infinilight[g.infinilightmax].e=t.tle;
				t.entityelement[t.tle].eleprof.light.index=g.infinilightmax;
			}
		}
		else
		{
			t.entityelement[t.tle].eleprof.light.islit=0;
			t.entityelement[t.tle].eleprof.light.index=0;
		}
	}
//  `exit prompt t$,""

//  `end


	//  clear old infini light history
	t.nearestlightindex[0]=0;
	t.nearestlightindex[1]=0;
	t.nearestlightindex[2]=0;
	t.nearestlightindex[3]=0;
	t.nearestlightindex[4]=0;

return;
}
