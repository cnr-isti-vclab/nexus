#ifndef NX_RENDERER_H
#define NX_RENDERER_H

#include <vector>
#include <wrap/system/time/clock.h>

#include "traversal.h"
#include "metric.h"
#include "nexus.h"



namespace nx {


// Attribute index.
enum {
	ATTRIB_VERTEX = 0,
	ATTRIB_NORMAL = 1,
	ATTRIB_COLOR = 2,
	ATTRIB_TEXTCOORD = 3,
	NUM_ATTRIBUTES = 4
};

class Nexus;
class Controller;

class Stats {
public:
	Stats() { resetAll(); }
	void resetAll();
	void resetCurrent();

	uint32_t rendered;      //number of primitives rendered per frame.
	float error;            //max error globally
	float fps;

	uint32_t patch_rendered;
	uint32_t node_rendered;       //inside view frustum
	uint32_t frustum_culled;      //
	uint32_t cone_culled;         //patches pointing away from the viewer.
	uint32_t occlusion_culled;    //patches removed by oclcusion culling

	uint32_t instance_rendered;   //number of primitives rendered in this instance
	float instance_error;         //max error in this instance

	mt::Clock time;
};


class Renderer: public Traversal {
public:
	enum Mode { TRIANGLES = 0x1, NORMALS = 0x2, COLORS = 0x4, TEXTURES = 0x8, PATCHES = 0x10, SPHERES = 0x20 };

	Metric metric;
	uint32_t mode;
	//Instance *instance;
	bool cone_culling;
	bool frustum_culling;        //perform frustum culling on patches
	bool occlusion_culling;

	float target_error, target_fps;
	uint32_t max_rendered;//unused at the moment

	Stats stats;

	Renderer():
		mode(TRIANGLES | NORMALS | COLORS | TEXTURES),
		cone_culling(false), frustum_culling(true), occlusion_culling(false),
		target_error(3.0f), max_rendered(0), controller(NULL), frame(0) {}

	void startFrame();
	void getView(const float *proj = NULL, const float *modelview = NULL, const int *viewport = NULL);
	void nearFar(Nexus *instance, float &near_distance, float &far_distance);
	void render(Nexus *instance, bool getview = true, int wait = 0);
	void endFrame();

	void setMode(Mode mode, bool on);
	void setConeCulling(bool on) { cone_culling = on; }
	void setFrustumCulling(bool on) { frustum_culling = on; }
	void setOcclusionCulling(bool on) { occlusion_culling = on; }

	void setFps(float fps) { target_fps = fps; }
	void setError(float error) { target_error = error; }
	void setMaxPrimitives(uint32_t t) { max_rendered = t; }
	void resetStats() { stats.resetAll(); }

protected:
	Controller *controller;
	//    uint32_t current_rendered;

	uint32_t frame;                //enough for 4 years of 30fps.

	uint32_t last_node; //max index of a node selected;
	std::vector<float> errors;

	Action expand(HeapNode h);
	float nodeError(uint32_t n, bool &visible);

	void renderSelected(Nexus *nexus);

	std::vector<nx::Token *> locked; //to unlock at the end of the function
};



} //namespace

#endif // RENDERER_H
