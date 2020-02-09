#pragma once

#include "pgapi.h"
#include "atlstr.h"
#include "hps.h"
#include <unordered_map>

enum axis { X, Y, Z };

static void handle_pg_error(PTStatus status, char *err_string)
{
	PTStatus status_code;
	PTStatus err_code;
	PTStatus func_code;
	PTStatus fatal_error = PV_STATUS_OK;
	CString msg;

	/* The status is made up of 3 parts */
	status_code = PM_STATUS_FROM_API_ERROR_CODE(status);
	func_code = PM_FN_FROM_API_ERROR_CODE(status);
	err_code = PM_ERR_FROM_API_ERROR_CODE(status);
	if (status_code & PV_STATUS_BAD_CALL)
	{
		msg.Format(_T("PG:BAD_CALL: Function %d Error %d: %s\n"), func_code, err_code, CString(err_string));
	}
	if (status_code & PV_STATUS_MEMORY)
	{
		msg.Format(_T("PG:MEMORY: Function %d Error %d: %s\n"), func_code, err_code, CString(err_string));
		fatal_error |= status;
	}
	if (status_code & PV_STATUS_EXCEPTION)
	{
		msg.Format(_T("PG:EXCEPTION: Function %d Error %d: %s\n"), func_code, err_code, CString(err_string));
		fatal_error |= status;
	}
	if (status_code & PV_STATUS_FILE_IO)
	{
		msg.Format(_T("PG:FILE I/0: Function %d Error %d: %s\n"), func_code, err_code, CString(err_string));
	}
	if (status_code & PV_STATUS_INTERRUPT)
	{
		msg.Format(_T("PG:INTERRUPT: Function %d Error %d: %s\n"), func_code, err_code, CString(err_string));
	}
	if (status_code & PV_STATUS_INTERNAL_ERROR)
	{
		msg.Format(_T("PG:INTERNAL_ERROR: Function %d Error %d: %s\n"), func_code, err_code, CString(err_string));
		fatal_error |= status;
	}

	//AfxMessageBox(msg);
}

struct PgMapItem
{
	HPS::ShellKey shellKey;
	PTSolid solid;

	PgMapItem(HPS::ShellKey _key, PTSolid _solid) : shellKey(_key), solid(_solid) {}
	PgMapItem() {}
};

struct ShellToSolidParams
{
	HPS::ShellKey shellKey;
	PTSolid solid;
	PTEnvironment pg_environment;
};

struct GetPgErrorsParams
{
	PTSolid solid;
	PTEnvironment pg_environment;
	HPS::SegmentKey segKey;
	bool bErrorsExist;
};

struct ShellBuilder
{
	HPS::PointArray points;
	HPS::IntArray faces;
	int current_face_index;
	//const CUPDUPDATA* pCUPDUPData = NULL;
	int progress;
};

struct AutoHealParams
{
	PTSolid solid;
	PTEnvironment pg_environment;
};

struct SolidToShellParams
{
	PTSolid solid;
	PTEnvironment pg_environment;
	HPS::ShellKit shellKit;
};

struct MeshPointCloudParams
{
	PTSolid solid;
	PTEnvironment pg_environment;
	HPS::SegmentKey segKey;
};

struct SimplifyParams
{
	PTSolid solid;
	int min_num_faces;
	bool bAvoidSelfIntersections;
};

struct HollowOutParams
{
	PTSolid solid;
	PTSolid inner;
	float amount;
};

struct SliceParams
{
	PTSolid solid;
	PTPoint origin;
	HPS::SegmentKey segKey;
	float step;
	int sliceCount;
	axis direction;
};

struct LatticeParams
{
	PTEnvironment pg_environment;
	PTSolid solid;
	float fStep;
	float rRadius;
	PTPoint min;
	PTPoint max;
	PTLattice lattice;
	axis direction;
};

struct FeatureRecognitionParams
{
	PTSolid solid;
	HPS::SegmentKey segKey;
	int planes = 0;
	int spheres = 0;
	int cylinders = 0;
	int cones = 0;
	int tori = 0;
};

typedef struct
{
	PTPoint *vertices;
	PTNat32 *segment_indices;
	PTNat32  num_vertices;
	PTNat32  num_segments;
} IndexedLattice;

CString GetUnitsAbreviation(HPS::UTF8 units);

bool ShellToSolid(ShellToSolidParams* pgArgs);

bool GetPgErrors(GetPgErrorsParams* getPgErrorsParams);

bool AutoHeal(AutoHealParams* autoHealParams, PTPointer appData);

bool SolidToShell(SolidToShellParams* solidToShellParams);

bool MeshPointCloud(MeshPointCloudParams* meshPointCloudParams);

bool Simplify(SimplifyParams* simplifyParams);

bool HollowOut(HollowOutParams* hollowOutParams);

bool Slice(SliceParams* sliceParams);

bool FeatureRecognition(FeatureRecognitionParams* featureRecognitionParams);

PTStatus mesh_begin(PTPointer app_data,
	PTNat32 num_faces,
	PTNat32 num_loops,
	PTNat32 num_vertex_indices,
	PTNat32 num_vertices,
	PTNat32 vertex_array_size,
	PTPoint **vertices,
	PTPointer **vertex_app_data);

PTStatus mesh_add_triangle(PTMeshPolygon *polygon);

PTStatus mesh_end(PTPointer app_data);

PTStatus EntityListToShell(PTEntityList face_list, HPS::ShellKit &shellKit);

void CombineShells(HPS::Key key);

bool GenerateLattice(HPS::SegmentKey root, std::unordered_map<intptr_t, PgMapItem> pg_map, const HWND hwnd, PTEnvironment environment, PTSolid *inner_solid_array, HPS::UTF8 units);
