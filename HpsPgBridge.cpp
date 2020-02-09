#include "stdafx.h"
#include "HpsPgBridge.h"

CString GetUnitsAbreviation(HPS::UTF8 units)
{
	if (units.Empty())
		return "";

	if (units == "Millimeter")
		return "mm";
	else if (units == "Centimeter")
		return "cm";
	else if (units == "Meter")
		return "m";
	else if (units == "Kilometer")
		return "km";
	else if (units == "Inch")
		return "in";
	else if (units == "Foot")
		return "ft";
	else if (units == "Yard")
		return "yd";
	else if (units == "Mile")
		return "mi";
	else
		return "";
}

bool ShellToSolid(ShellToSolidParams* pgArgs)
{
	//ShellToSolidParams* pgArgs = (ShellToSolidParams*)pCUPDUPData->GetAppData();
	
	//pCUPDUPData->SetProgress(_T("Parsing Faces.."), 0);

	size_t faceCount = pgArgs->shellKey.GetFaceCount();
	HPS::IntArray faceList;
	pgArgs->shellKey.ShowFacelist(faceList);
	PTNat32* pg_faces = new PTNat32[faceCount * 3];
	PTStatus status = PFThreadRegister();

	int progress = 0;

	for (int i = 0; i < faceCount; i++)
	{
		pg_faces[i * 3] = faceList[i * 4 + 1];
		pg_faces[i * 3 + 1] = faceList[i * 4 + 2];
		pg_faces[i * 3 + 2] = faceList[i * 4 + 3];

		if (i * 100 / faceCount > progress)
		{
			progress = i * 100 / faceCount;
			//pCUPDUPData->SetProgress(_T("Parsing Faces.."), progress);
		}
	}

	HPS::PointArray pointList;
	pgArgs->shellKey.ShowPoints(pointList);
	PTPoint* vertices = new PTPoint[pointList.size()];

	progress = 0;

	for (int i = 0; i < pointList.size(); i++)
	{
		vertices[i][0] = pointList[i].x;
		vertices[i][1] = pointList[i].y;
		vertices[i][2] = pointList[i].z;

		if (i * 100 / pointList.size() > progress)
		{
			progress = i * 100 / pointList.size();
			//pCUPDUPData->SetProgress(_T("Parsing Points.."), progress);
		}
	}

	//pCUPDUPData->SetProgress(_T("Creating PG Solid.."), 50);
	status = PFSolidCreateFromMesh(
		pgArgs->pg_environment,
		faceCount,	// total number of triangles
		NULL,		// no internal loops
		NULL,		// all faces are triangles
		pg_faces,	// indices into vertex array
		vertices,		// pointer to vertex array
		NULL,		// no options
		&pgArgs->solid		// resultant pg solid
	);

	status = PFThreadUnregister();
	return true;
}

bool GetPgErrors(GetPgErrorsParams* getPgErrorsParams)
{
	PTStatus status = PFThreadRegister();

	//GetPgErrorsParams* getPgErrorsParams = (GetPgErrorsParams*)pCUPDUPData->GetAppData();

	getPgErrorsParams->segKey.Subsegment("pg_errors").GetDrawingAttributeControl().SetFaceDisplacement(true, -1);

	/// FIND OPEN EDGES ///
	PTCategory openEdgesCategory;
	status = PFCategoryCreate(
		getPgErrorsParams->pg_environment,
		PV_CRITERION_OPEN_EDGES,
		NULL, // no options necessary
		&openEdgesCategory
	);

	PTEntityGroup openEdgesEntityGroup;
	status = PFEntityGroupCreateFromCategory(
		getPgErrorsParams->solid,
		openEdgesCategory,
		&openEdgesEntityGroup
	);

	PTEntityList openEdgeList;
	status = PFEntityCreateEntityList(openEdgesEntityGroup, PV_ENTITY_TYPE_EDGE, NULL, &openEdgeList);

	PTEntity aEntity;
	PTEdge eEdge;
	PTVertex vFrom, vTo;
	PTPoint vFromPos, vToPos;
	int iEntity;

	if ((aEntity = PFEntityListGetFirst(openEdgeList)))
	{
		HPS::SegmentKey pgOpenEdgesSegment = getPgErrorsParams->segKey.Subsegment("pg_errors").Subsegment("OpenEdges");
		pgOpenEdgesSegment.GetVisibilityControl().SetLines(true);
		pgOpenEdgesSegment.GetMaterialMappingControl().SetLineColor(HPS::RGBColor(0, 255, 0));
		pgOpenEdgesSegment.GetLineAttributeControl().SetWeight(2);
		HPS::AttributeLockTypeArray lockTypeArray;
		lockTypeArray.push_back(HPS::AttributeLock::Type::MaterialLineColor);
		HPS::BoolArray lockStatus(1, true);
		pgOpenEdgesSegment.GetAttributeLockControl().SetLock(lockTypeArray, lockStatus);

		iEntity = 0;
		while ((eEdge = (PTEdge)aEntity))
		{
			iEntity++;
			PFEdgeGetVertices(eEdge, &vFrom, &vTo);
			PFEntityGetPointProperty(vFrom, PV_VERTEX_PROP_POINT, vFromPos);
			PFEntityGetPointProperty(vTo, PV_VERTEX_PROP_POINT, vToPos);
			pgOpenEdgesSegment.InsertLine(
				HPS::Point(vFromPos[0], vFromPos[1], vFromPos[2]),
				HPS::Point(vToPos[0], vToPos[1], vToPos[2])
			);

			aEntity = PFEntityListGetNext(openEdgeList, aEntity);
		}
	}

	status = PFEntityGroupDestroy(openEdgesEntityGroup);
	status = PFCategoryInvalidate(openEdgesCategory);

	/// FIND SELF INTERSECTIONS ///
	PTCategory selfIntersectionsCategory;
	status = PFCategoryCreate(
		getPgErrorsParams->pg_environment,
		PV_CRITERION_SELF_INT_FACES,
		NULL, // no options necessary
		&selfIntersectionsCategory
	);

	PTEntityGroup selfIntersectionsEntityGroup;
	status = PFEntityGroupCreateFromCategory(
		getPgErrorsParams->solid,
		selfIntersectionsCategory,
		&selfIntersectionsEntityGroup
	);

	PTEntityList selfIntersectionsList;
	status = PFEntityCreateEntityList(selfIntersectionsEntityGroup, PV_ENTITY_TYPE_FACE, NULL, &selfIntersectionsList);

	HPS::ShellKit shellKit;
	EntityListToShell(selfIntersectionsList, shellKit);

	HPS::SegmentKey pgSelfIntersectSegment = getPgErrorsParams->segKey.Subsegment("pg_errors").Subsegment("SelfIntersections");
	pgSelfIntersectSegment.GetMaterialMappingControl().SetFaceColor(HPS::RGBColor(255, 0, 0));
	HPS::AttributeLockTypeArray lockTypeArray;
	lockTypeArray.push_back(HPS::AttributeLock::Type::MaterialFace);
	HPS::BoolArray lockStatus(1, true);
	pgSelfIntersectSegment.GetAttributeLockControl().SetLock(lockTypeArray, lockStatus);

	pgSelfIntersectSegment.InsertShell(shellKit);
	
	status = PFEntityGroupDestroy(selfIntersectionsEntityGroup);
	status = PFCategoryInvalidate(selfIntersectionsCategory);

	/// FIND NON-MANIFOLD EDGES ///
	PTCategory nonManifoldCategory;
	status = PFCategoryCreate(
		getPgErrorsParams->pg_environment,
		PV_CRITERION_NON_MANIFOLD_EDGES,
		NULL, // no options necessary
		&nonManifoldCategory
	);

	PTEntityGroup nonManifoldEntityGroup;
	status = PFEntityGroupCreateFromCategory(
		getPgErrorsParams->solid,
		nonManifoldCategory,
		&nonManifoldEntityGroup
	);

	PTEntityList nonManifoldList;
	status = PFEntityCreateEntityList(nonManifoldEntityGroup, PV_ENTITY_TYPE_EDGE, NULL, &nonManifoldList);

	if ((aEntity = PFEntityListGetFirst(nonManifoldList)))
	{
		HPS::SegmentKey pgNonManifoldSegment = getPgErrorsParams->segKey.Subsegment("pg_errors").Subsegment("NonManifold");
		pgNonManifoldSegment.GetVisibilityControl().SetLines(true);
		pgNonManifoldSegment.GetMaterialMappingControl().SetLineColor(HPS::RGBColor(255, 0, 255)); // magenta?
		pgNonManifoldSegment.GetLineAttributeControl().SetWeight(2);
		HPS::AttributeLockTypeArray lockTypeArray;
		lockTypeArray.push_back(HPS::AttributeLock::Type::MaterialLineColor);
		HPS::BoolArray lockStatus(1, true);
		pgNonManifoldSegment.GetAttributeLockControl().SetLock(lockTypeArray, lockStatus);

		iEntity = 0;
		while ((eEdge = (PTEdge)aEntity))
		{
			iEntity++;
			PFEdgeGetVertices(eEdge, &vFrom, &vTo);
			PFEntityGetPointProperty(vFrom, PV_VERTEX_PROP_POINT, vFromPos);
			PFEntityGetPointProperty(vTo, PV_VERTEX_PROP_POINT, vToPos);
			pgNonManifoldSegment.InsertLine(
				HPS::Point(vFromPos[0], vFromPos[1], vFromPos[2]),
				HPS::Point(vToPos[0], vToPos[1], vToPos[2])
			);

			aEntity = PFEntityListGetNext(nonManifoldList, aEntity);
		}
	}

	status = PFEntityGroupDestroy(nonManifoldEntityGroup);
	status = PFCategoryInvalidate(nonManifoldCategory);

	status = PFThreadUnregister();

	return true;
}

int currentPercent = -1;

PTStatus progress_cb(PTProgressReport progress)
{
	double percent;
	percent = PFEntityGetDoubleProperty(progress, PV_PROGRESS_PROP_STAGE_PERCENT);

	if ((int)percent == currentPercent) return PV_STATUS_OK; // if the percentage hasn't changed don't do anything

	PTOperationType operation;
	char operation_name[256];
	std::wstring str;

	operation = (PTOperationType)PFEntityGetEnumProperty(progress, PV_PROGRESS_PROP_OPERATION_ID);
	switch (operation) {
	case PV_OPERATION_CLIP_LATTICE:
		str = _T("Clipping Lattice"); break;
	case PV_OPERATION_CLOSE_SOLID:
		str = _T("Closing solid"); break;
	case PV_OPERATION_CLOSE_SURFACE:
		str = _T("Closing surface"); break;
	case PV_OPERATION_COMPARE_SOLIDS:
		str = _T("Comparing solids"); break;
	case PV_OPERATION_FIX_SOLID_SELF_INT:
		str = _T("Fixing self intersections"); break;
	case PV_OPERATION_OFFSET_SOLID:
		str = _T("Offsetting solid"); break;
	case PV_OPERATION_SET_PRECISION:
		str = _T("Making manifold"); break;
	case PV_OPERATION_SIMPLIFY_SOLID:
		str = _T("Simplifying solid"); break;
	case PV_OPERATION_SLICE_SOLID:
		str = _T("Slicing solid"); break;
	default:
		str = _T("Polygonica is thinking"); break;
	}
	
	PTPointer ptPointer = PFEntityGetPointerProperty(progress, PV_PROGRESS_PROP_APP_DATA);
	if (ptPointer)
	{
		currentPercent = (int)percent;
		//((CUPDUPDATA*)ptPointer)->SetProgress(str.c_str(), currentPercent);
	}
	return PV_STATUS_OK;
}

bool AutoHeal(AutoHealParams* autoHealParams, PTPointer appData)
{
	PTStatus status = PFThreadRegister();

	//AutoHealParams* autoHealParams = (AutoHealParams*)pCUPDUPData->GetAppData();
	PTSolid solid = autoHealParams->solid;

	PTBoolean solid_is_closed = PFEntityGetBooleanProperty(solid, PV_SOLID_PROP_CLOSED);
	if (!solid_is_closed)
	{
		PTSolidCloseOpts closeOpts;
		PMInitSolidCloseOpts(&closeOpts);
		closeOpts.progress_callback = progress_cb;
		closeOpts.app_data = appData; // (PTPointer)pCUPDUPData;

		status = PFSolidClose(solid, &closeOpts);
		solid_is_closed = PFEntityGetBooleanProperty(solid, PV_SOLID_PROP_CLOSED);
	}

	PTBoolean solid_has_bad_orientation = PFEntityGetBooleanProperty(solid, PV_SOLID_PROP_BAD_ORIENTATION); // not really needed if we are closing the solid, but what if the solid is closed and the orientationis bad? TODO, fix this!
	// it may be closed, but still have self intersections, so add code here
	if( !solid_has_bad_orientation )
	{
		
	}

	PTBoolean solid_has_self_intersects = PFEntityGetBooleanProperty(solid, PV_SOLID_PROP_SELF_INTERSECTS);
	if (solid_has_self_intersects)
	{
		PTSolidFixSelfIntsOpts selfIntsOpts;
		PMInitSolidFixSelfIntsOpts(&selfIntsOpts);
		selfIntsOpts.progress_callback = progress_cb;
		selfIntsOpts.app_data = appData; // (PTPointer)pCUPDUPData;

		status = PFSolidFixSelfIntersections(solid, &selfIntsOpts);
		solid_has_self_intersects = PFEntityGetBooleanProperty(solid, PV_SOLID_PROP_SELF_INTERSECTS);
	}

	//pCUPDUPData->SetProgress(_T("Making manifold.."), 66);

	PTBoolean solid_is_mainfold = PFEntityGetBooleanProperty(solid, PV_SOLID_PROP_MANIFOLD);
	if (!solid_is_mainfold)
	{
		PTSolidSetPrecisionOpts options;
		PMInitSolidSetPrecisionOpts(&options);
		options.make_manifold = true;
		PTPrecisionType flags = PV_PREC_TYPE_SINGLE;
		options.progress_callback = progress_cb;
		options.app_data = appData; // (PTPointer)pCUPDUPData;
		status = PFSolidSetPrecision(solid, flags, &options);
		solid_is_mainfold = PFEntityGetBooleanProperty(solid, PV_SOLID_PROP_MANIFOLD);
	}

	status = PFThreadUnregister();

	return true;

}

PTStatus mesh_begin(PTPointer app_data,
	PTNat32 num_faces,
	PTNat32 num_loops,
	PTNat32 num_vertex_indices,
	PTNat32 num_vertices,
	PTNat32 vertex_array_size,
	PTPoint **vertices,
	PTPointer **vertex_app_data)
{
	ShellBuilder* shellBuilder = (ShellBuilder*)app_data;

	PTPoint ptPos;
	shellBuilder->points.resize(num_vertices);
	shellBuilder->faces.resize(num_faces * 4);
	shellBuilder->current_face_index = 0;
	shellBuilder->progress = 0;

	for (PTNat32 n = 0; n < num_vertices; n++)
	{
		PFMeshGetVertexPosition(ptPos, vertices, vertex_array_size, n);
		shellBuilder->points[n] = HPS::Point((float)ptPos[0], (float)ptPos[1], (float)ptPos[2]);
	}

	return PV_STATUS_OK;
};

PTStatus mesh_add_triangle(PTMeshPolygon *polygon)
{
	ShellBuilder* shellBuilder = (ShellBuilder*)polygon->options->app_data;
	
	int current_progress = (shellBuilder->current_face_index * 100) / shellBuilder->faces.size();
	if (current_progress > shellBuilder->progress)
	{
		//shellBuilder->pCUPDUPData->SetProgress(_T("Creating shell"), current_progress);
		shellBuilder->progress = current_progress;
	}

	int v0 = polygon->indices[0];
	int v1 = polygon->indices[1];
	int v2 = polygon->indices[2];

	shellBuilder->faces[shellBuilder->current_face_index] = 3;
	shellBuilder->faces[shellBuilder->current_face_index + 1] = v0;
	shellBuilder->faces[shellBuilder->current_face_index + 2] = v1;
	shellBuilder->faces[shellBuilder->current_face_index + 3] = v2;
	shellBuilder->current_face_index += 4;

	return PV_STATUS_OK;
}

PTStatus mesh_end(PTPointer app_data)
{
	return PV_STATUS_OK;
}

bool SolidToShell(SolidToShellParams* solidToShellParams)
{
	PTStatus status = PFThreadRegister();

	//SolidToShellParams* solidToShellParams = (SolidToShellParams*)pCUPDUPData->GetAppData();
	PTSolid solid = solidToShellParams->solid;

	ShellBuilder shellBuilder;
	//shellBuilder.pCUPDUPData = pCUPDUPData; //! Todo: Not sure if this is used later or not

	// Get the data from the solid and repopulate the shell!
	PTGetMeshOpts options;
	PMInitGetMeshOpts(&options);
	options.app_data = &shellBuilder;
	options.begin_callback = mesh_begin;
	options.add_polygon_callback = &mesh_add_triangle;
	options.end_callback = &mesh_end;

	options.output_vertex_normals = TRUE;
	options.output_face_app_data = TRUE;
	options.output_face_colours = FALSE;

	PFSolidGetMesh(solid, PV_MESH_TRIANGLES, &options);

	solidToShellParams->shellKit.SetFacelist(shellBuilder.faces);
	solidToShellParams->shellKit.SetPoints(shellBuilder.points);


	status = PFThreadUnregister();

	return true;
}

bool MeshPointCloud(MeshPointCloudParams* meshPointCloudParams)
{
	PTStatus status = PFThreadRegister();

	//MeshPointCloudParams* meshPointCloudParams = (MeshPointCloudParams*)pCUPDUPData->GetAppData();

	PTPointCloud cloud;
	//status = PFPointCloudCreateBegin(meshPointCloudParams->pg_environment, NULL, &cloud);
	status = PFPointCloudCreate(meshPointCloudParams->pg_environment, NULL, &cloud);

	PTPointSet set;
	PTCreatePointSetOpts options;
	options.viewpoint = PV_ENTITY_NULL;
	status = PFPointSetCreate(
		cloud,
		NULL, // PTTransformMatrix transform
		&options,
		&set
	);

	HPS::SegmentKey rootSegmentKey = meshPointCloudParams->segKey;

	HPS::SearchResults searchResults;
	size_t numResults = rootSegmentKey.Find(HPS::Search::Type::Shell,        // searching for shells
		HPS::Search::Space::SubsegmentsAndIncludes,  // within all subsegments
		searchResults);                   // search results returned here
	HPS::SearchResultsIterator it = searchResults.GetIterator();
	while (it.IsValid())
	{
		HPS::Key key = it.GetItem();

		if (key.Type() == HPS::Type::ShellKey)
		{
			HPS::ShellKey shellKey = (HPS::ShellKey)key;

			PTNat32 npoints = shellKey.GetPointCount();
			PTPoint* points = new PTPoint[npoints];
			HPS::PointArray pointList;
			shellKey.ShowPoints(pointList);

			for (int i = 0; i < npoints; i++)
			{
				points[i][0] = pointList[i].x;
				points[i][1] = pointList[i].y;
				points[i][2] = pointList[i].z;
			}

			PTPointSetOpts psoptions; // TODO: can use single precision so we don't have to copy
			PMInitPointSetOpts(&psoptions);
			status = PFPointSetAddPoints(
				set,
				npoints,
				points,
				&psoptions
			);
		}
		it.Next();
	}

	//status = PFPointSetCreateEnd(set);
	//status = PFPointCloudCreateEnd(cloud);

	// mesh the cloud
	PTSolid solid;
	PTPointCloudSolidOpts solid_options;
	solid_options.progress_callback = progress_cb;
	//solid_options.app_data = (PTPointer)pCUPDUPData;

	PMInitPointCloudSolidOpts(&solid_options);
	status = PFSolidCreateFromPointCloud(
		cloud,
		&solid_options,
		&solid
	);

	meshPointCloudParams->solid = solid;

	status = PFThreadUnregister();

	return true;
}

bool Simplify(SimplifyParams* simplifyParams)
{
	PTStatus status = PFThreadRegister();

	//SimplifyParams* simplifyParams = (SimplifyParams*)pCUPDUPData->GetAppData();

	PTSolidSimplifyOpts options;
	PMInitSolidSimplifyOpts(&options);

	double actual_error = 0.0;

	options.simplify_limits = 0;
	options.simplify_limits |= PV_SIMPLIFY_LIMIT_NUM_FACES;
	options.actual_error = &actual_error;
	options.max_error = 0.1;
	options.min_num_faces = simplifyParams->min_num_faces;
	options.avoid_new_self_isects = false;
	options.type = PV_SIMPLIFY_MIXED;
	//options.app_data = (PTPointer)pCUPDUPData;
	options.avoid_new_self_isects = simplifyParams->bAvoidSelfIntersections;// TRUE;
	options.progress_callback = progress_cb;

	status = PFSolidSimplify(simplifyParams->solid, &options);

	status = PFThreadUnregister();

	return true;
}

bool HollowOut(HollowOutParams* hollowOutParams)
{
	PTStatus status = PFThreadRegister();

	//HollowOutParams* hollowOutParams = (HollowOutParams*)pCUPDUPData->GetAppData();

	PTSolidOffsetOpts options;
	PMInitSolidOffsetOpts(&options);
	//options.app_data = (PTPointer)pCUPDUPData;
	options.progress_callback = progress_cb;

	// Code that saves off the interior for later use
	options.keep_original_geometry = FALSE;

	PTSolid inner;
	status = PFSolidCopy(hollowOutParams->solid, NULL, &inner);

	status = PFSolidOffset(inner, hollowOutParams->amount, hollowOutParams->amount * -0.1, &options);
	if (status != 0)
	{
		char err[1024];
		handle_pg_error(status, err);
		status = PFThreadUnregister();
		return false;
	}

	hollowOutParams->inner = inner;
	// end code

	options.keep_original_geometry = TRUE;

	status = PFSolidOffset(hollowOutParams->solid, hollowOutParams->amount, hollowOutParams->amount * -0.1, &options);
	status = PFThreadUnregister();

	return true;
}

//bool Slice(const CUPDUPDATA* pCUPDUPData)
//{
//	PTStatus status = PFThreadRegister();
//
//	SliceParams* sliceParams = (SliceParams*)pCUPDUPData->GetAppData();
//	PTVector xp_axis = { 1, 0, 0 };
//	PTVector zp_axis = { 0, 0, 1 };
//	double z_vals[100];
//	for (auto i = 0; i < 100; i++) z_vals[i] = i*sliceParams->step;
//	PTSliceOpts options;
//	PMInitSliceOpts(&options);
//	PTProfile profiles[100];
//
//	HPS::SegmentKey sliceSeg = sliceParams->segKey;
//
//	sliceSeg.GetVisibilityControl().SetLines(true);
//	sliceSeg.GetMaterialMappingControl().SetLineColor(HPS::RGBColor(255, 255, 255));
//	sliceSeg.GetLineAttributeControl().SetWeight(2);
//	HPS::AttributeLockTypeArray lockTypeArray;
//	lockTypeArray.push_back(HPS::AttributeLock::Type::MaterialLineColor);
//	HPS::BoolArray lockStatus(1, true);
//	sliceSeg.GetAttributeLockControl().SetLock(lockTypeArray, lockStatus);
//
//	status = PFSolidCreateSlices(sliceParams->solid,
//		100, //numslices
//		sliceParams->origin, //origin
//		xp_axis, //xaxis
//		zp_axis, //yaxis
//		z_vals, //zvals
//		&options, //options
//		profiles); // profiles
//
//	for (int i = 0; i < 100; i++)
//	{
//		PTProfileLoop loop;
//		PTProfileEdge edge, first_edge;
//		PTCurveVertex start_vertex, end_vertex;
//		PTPoint start_point, end_point;
//		
//		int j = 0;
//
//		for (loop = PFProfileGetFirstLoop(profiles[i]); loop; loop = PFProfileLoopGetNextLoop(loop))
//		{ 
//			HPS::PointArray ptArray;
//			if (loop != 0)
//			{
//				first_edge = PFProfileLoopGetFirstEdge(loop);
//				edge = PFProfileLoopGetFirstEdge(loop);
//				do {
//					j++;
//					PTEnum edge_enum = PFEntityGetEnumProperty(edge, PV_PRFL_EDGE_PROP_TYPE);
//					switch (edge_enum)
//					{
//					case PV_PROFILE_EDGE_TYPE_ARC:
//						/*edge_type = "arc";
//						arc_angle = PFEntityGetDoubleProperty(edge, PV_PRFL_EDGE_PROP_ARC_ANGLE);
//						printf("Edge %4d %6s %.0f degrees\n ", j, edge_type, arc_angle);
//						start_vertex = PFProfileEdgeGetStartVertex(edge);
//						PFEntityGetPointProperty(start_vertex, PV_PRFL_VERT_PROP_POINT, start_point);
//						end_vertex = PFProfileEdgeGetFinishVertex(edge);
//						PFEntityGetPointProperty(end_vertex, PV_PRFL_VERT_PROP_POINT, end_point);
//						printf("to (%.3f, %.3f) \n", end_point[0], end_point[1]);*/
//						ASSERT(0);
//
//						break;
//
//					case PV_PROFILE_EDGE_TYPE_LINE:
//						//edge_type = "line";
//						//printf("Edge %4d %6s\n ", j, edge_type);
//						start_vertex = PFProfileEdgeGetStartVertex(edge);
//						PFEntityGetPointProperty(start_vertex, PV_PRFL_VERT_PROP_POINT, start_point);
//						ptArray.push_back(HPS::Point(start_point[0] + sliceParams->origin[0], start_point[1] + sliceParams->origin[1], sliceParams->origin[2] + i*sliceParams->step));
//
//						end_vertex = PFProfileEdgeGetFinishVertex(edge);
//						PFEntityGetPointProperty(end_vertex, PV_PRFL_VERT_PROP_POINT, end_point);
//						//printf("to (%.3f, %.3f) \n", end_point[0], end_point[1]);
//						break;
//
//					default:
//						//edge_type = "unknown";
//						ASSERT(0);
//						break;
//					}
//					edge = PFProfileEdgeGetNextEdge(edge);
//				} while (edge && edge != first_edge);
//				ptArray.push_back(HPS::Point(end_point[0] + sliceParams->origin[0], end_point[1] + sliceParams->origin[1], sliceParams->origin[2] + i*sliceParams->step));
//				sliceSeg.InsertLine(ptArray);
//			}
//		}
//	}
//
//	status = PFThreadUnregister();
//	return true;
//
//}
bool Slice(SliceParams* sliceParams)
{
	PTStatus status = PFThreadRegister();

	//SliceParams* sliceParams = (SliceParams*)pCUPDUPData->GetAppData();
	PTVector xp_axis = { 0, 0, 0 };
	PTVector zp_axis = { 0, 0, 0 }; // normal vector


	switch (sliceParams->direction)
	{
	case X:
		xp_axis[1] = 1; //PTVector xp_axis = { 0, 1, 0 };
		zp_axis[0] = 1; //PTVector zp_axis = { 1, 0, 0 }
		break;
	case Y:
		xp_axis[0] = 1; //PTVector xp_axis = { 0, 1, 0 };
		zp_axis[1] = 1; //PTVector zp_axis = { 0, 1, 0 }
		break;
	case Z:
		xp_axis[0] = 1;
		zp_axis[2] = 1;
		break;
	}

	double *z_vals = (double*)malloc(sliceParams->sliceCount * sizeof(double));;
	for (auto i = 0; i < sliceParams->sliceCount; i++) z_vals[i] = i*sliceParams->step;
	PTSliceOpts options;
	PMInitSliceOpts(&options);

	//options.app_data = (PTPointer)pCUPDUPData;
	options.progress_callback = progress_cb;

	PTProfile *profiles = (PTProfile*)malloc(sliceParams->sliceCount * sizeof(PTProfile));

	HPS::SegmentKey sliceSeg = sliceParams->segKey;

	// Clear out old geometry if it exists
	sliceSeg.Flush(HPS::Search::Type::Geometry);

	sliceSeg.GetVisibilityControl().SetLines(true);
	sliceSeg.GetMaterialMappingControl().SetLineColor(HPS::RGBColor(255, 255, 255));
	sliceSeg.GetLineAttributeControl().SetWeight(2);
	HPS::AttributeLockTypeArray lockTypeArray;
	lockTypeArray.push_back(HPS::AttributeLock::Type::MaterialLineColor);
	HPS::BoolArray lockStatus(1, true);
	sliceSeg.GetAttributeLockControl().SetLock(lockTypeArray, lockStatus);

	status = PFSolidCreateSlices(sliceParams->solid,
		sliceParams->sliceCount, //numslices
		sliceParams->origin, //origin
		xp_axis, //xaxis
		zp_axis, //yaxis
		z_vals, //zvals
		&options, //options
		profiles); // profiles

	for (int i = 0; i < sliceParams->sliceCount; i++)
	{
		PTProfileLoop loop;
		PTProfileEdge edge, first_edge;
		PTCurveVertex start_vertex, end_vertex;
		PTPoint start_point, end_point;

		int j = 0;

		for (loop = PFProfileGetFirstLoop(profiles[i]); loop; loop = PFProfileLoopGetNextLoop(loop))
		{
			HPS::PointArray ptArray;
			if (loop != 0)
			{
				first_edge = PFProfileLoopGetFirstEdge(loop);
				edge = PFProfileLoopGetFirstEdge(loop);
				do {
					j++;
					PTEnum edge_enum = PFEntityGetEnumProperty(edge, PV_PRFL_EDGE_PROP_TYPE);
					switch (edge_enum)
					{
					case PV_PROFILE_EDGE_TYPE_ARC:
						/*edge_type = "arc";
						arc_angle = PFEntityGetDoubleProperty(edge, PV_PRFL_EDGE_PROP_ARC_ANGLE);
						printf("Edge %4d %6s %.0f degrees\n ", j, edge_type, arc_angle);
						start_vertex = PFProfileEdgeGetStartVertex(edge);
						PFEntityGetPointProperty(start_vertex, PV_PRFL_VERT_PROP_POINT, start_point);
						end_vertex = PFProfileEdgeGetFinishVertex(edge);
						PFEntityGetPointProperty(end_vertex, PV_PRFL_VERT_PROP_POINT, end_point);
						printf("to (%.3f, %.3f) \n", end_point[0], end_point[1]);*/
						
						//ASSERT(0);

						break;

					case PV_PROFILE_EDGE_TYPE_LINE:
						//edge_type = "line";
						//printf("Edge %4d %6s\n ", j, edge_type);
						start_vertex = PFProfileEdgeGetStartVertex(edge);
						PFEntityGetPointProperty(start_vertex, PV_PRFL_VERT_PROP_POINT, start_point);
						//ptArray.push_back(HPS::Point(start_point[0] + sliceParams->origin[0], start_point[1] + sliceParams->origin[1], sliceParams->origin[2] + i*sliceParams->step));
						//ptArray.push_back(HPS::Point(sliceParams->origin[2] + i*sliceParams->step, start_point[0] + sliceParams->origin[0], start_point[1] + sliceParams->origin[1]));

						switch (sliceParams->direction)
						{
						case X:
							ptArray.push_back(HPS::Point(sliceParams->origin[0] + i*sliceParams->step, start_point[0] + sliceParams->origin[1], start_point[1] + sliceParams->origin[2]));
							break;
						case Y:
							ptArray.push_back(HPS::Point(sliceParams->origin[0]  + start_point[0] , sliceParams->origin[1] + i*sliceParams->step, -start_point[1] + sliceParams->origin[2]));
							break;
						case Z:
							ptArray.push_back(HPS::Point(sliceParams->origin[0] + start_point[0], sliceParams->origin[1] + start_point[1], sliceParams->origin[2] + i*sliceParams->step));
							break;
						}
						end_vertex = PFProfileEdgeGetFinishVertex(edge);
						PFEntityGetPointProperty(end_vertex, PV_PRFL_VERT_PROP_POINT, end_point);
						//printf("to (%.3f, %.3f) \n", end_point[0], end_point[1]);
						break;

					default:
						//edge_type = "unknown";

						//ASSERT(0);

						break;
					}
					edge = PFProfileEdgeGetNextEdge(edge);
				} while (edge && edge != first_edge);
				//ptArray.push_back(HPS::Point(sliceParams->origin[2] + i*sliceParams->step, end_point[0] + sliceParams->origin[0], end_point[1] + sliceParams->origin[1]));
				switch (sliceParams->direction)
				{
				case X:
					ptArray.push_back(HPS::Point(sliceParams->origin[0] + i*sliceParams->step, end_point[0] + sliceParams->origin[1], end_point[1] + sliceParams->origin[2]));
					break;
				case Y:
					ptArray.push_back(HPS::Point(sliceParams->origin[0] + end_point[0], sliceParams->origin[1] + i*sliceParams->step, -end_point[1] + sliceParams->origin[2]));
					break;
				case Z:
					ptArray.push_back(HPS::Point(sliceParams->origin[0] + end_point[0], sliceParams->origin[1] + end_point[1], sliceParams->origin[2] + i*sliceParams->step));
					break;
				}
				sliceSeg.InsertLine(ptArray);
			}
		}
	}

	delete z_vals;
	delete profiles;

	status = PFThreadUnregister();
	return true;

}

bool FeatureRecognition(FeatureRecognitionParams* featureRecognitionParams)
{
	PTStatus status = PFThreadRegister();

	//FeatureRecognitionParams* featureRecognitionParams = (FeatureRecognitionParams*)pCUPDUPData->GetAppData();

	HPS::SegmentKey pgFeaturesSegment = featureRecognitionParams->segKey.Subsegment("pg_features");
	pgFeaturesSegment.GetVisibilityControl().SetFaces(true);

	HPS::SegmentKey planesSegment = pgFeaturesSegment.Subsegment("planes");
	planesSegment.GetMaterialMappingControl().SetFaceColor(HPS::RGBColor(1, 0, 0));

	HPS::SegmentKey spheresSegment = pgFeaturesSegment.Subsegment("spheres");
	spheresSegment.GetMaterialMappingControl().SetFaceColor(HPS::RGBColor(0, 1, 0));

	HPS::SegmentKey cylindersSegment = pgFeaturesSegment.Subsegment("cylinders");
	cylindersSegment.GetMaterialMappingControl().SetFaceColor(HPS::RGBColor(1, 1, 0));

	HPS::SegmentKey conesSegment = pgFeaturesSegment.Subsegment("cones");
	conesSegment.GetMaterialMappingControl().SetFaceColor(HPS::RGBColor(0, 0, 1));

	HPS::SegmentKey toriSegment = pgFeaturesSegment.Subsegment("tori");
	toriSegment.GetMaterialMappingControl().SetFaceColor(HPS::RGBColor(0, 1, 1));

	PTCreateSurfaceListOpts opts;
	PMInitCreateSurfaceListOpts(&opts);
	//opts.app_data = (PTPointer)pCUPDUPData;
	opts.progress_callback = progress_cb;

	PTEntityList surface_list;

	status = PFSolidCreateSurfaceList(
		featureRecognitionParams->solid,
		&opts,
		&surface_list
	);

	PTEntity entity = PFEntityListGetFirst(surface_list);
	while (entity != PV_ENTITY_NULL)
	{
		PTEntityGroup faces;
		status = PFSurfaceGetFaces(entity, &faces);
		PTEntityList face_list;

		status = PFEntityCreateEntityList(faces, PV_ENTITY_TYPE_FACE, NULL, &face_list);

		HPS::ShellKit shellKit;
		status = EntityListToShell(face_list, shellKit);

		PTSurfaceDef def;
		status = PFSurfaceGetDefinition(entity, &def);

		switch (def.type)
		{
		case PV_SURFACE_TYPE_PLANE:
			planesSegment.InsertShell(shellKit);
			featureRecognitionParams->planes++;
			break;
		case PV_SURFACE_TYPE_SPHERE_INSIDE:
		case PV_SURFACE_TYPE_SPHERE_OUTSIDE:
			spheresSegment.InsertShell(shellKit);
			featureRecognitionParams->spheres++;
			break;
		case PV_SURFACE_TYPE_CYLINDER_INSIDE:
		case PV_SURFACE_TYPE_CYLINDER_OUTSIDE:
			cylindersSegment.InsertShell(shellKit);
			featureRecognitionParams->cylinders++;
			break;
		case PV_SURFACE_TYPE_CONE_INSIDE:
		case PV_SURFACE_TYPE_CONE_OUTSIDE:
			conesSegment.InsertShell(shellKit);
			featureRecognitionParams->cones++;
			break;
		case PV_SURFACE_TYPE_TORUS_INSIDE:
		case PV_SURFACE_TYPE_TORUS_OUTSIDE:
			toriSegment.InsertShell(shellKit);
			featureRecognitionParams->tori++;
			break;
		}

		entity = PFEntityListGetNext(surface_list, entity);
	}

	status = PFEntityListDestroy(surface_list, 0);

	status = PFThreadUnregister();

	return true;
}

PTStatus EntityListToShell(PTEntityList face_list, HPS::ShellKit &shellKit)
{
	PTStatus status = 0;

	PTEntity aEntity = PFEntityListGetFirst(face_list);
	HPS::PointArray points;
	HPS::IntArray hpsfaces;
	int pointIndex = 0;

	while (aEntity != NULL)
	{
		PTFace face = aEntity;
		PTFaceEdgeData edgeData;
		PFFaceGetEdges(face, &edgeData);

		hpsfaces.push_back(3);

		for (int i = 0; i < edgeData.num_outer_edges; i++) // should be 3
		{
			PTEdge edge = edgeData.edges[i];
			bool isForward = edgeData.edge_is_forward[i];
			PTVertex vFrom, vTo;
			PTPoint vFromPos, vToPos;
			PFEdgeGetVertices(edge, &vFrom, &vTo);
			PFEntityGetPointProperty(vFrom, PV_VERTEX_PROP_POINT, vFromPos);
			PFEntityGetPointProperty(vTo, PV_VERTEX_PROP_POINT, vToPos);
			if (isForward)
				points.push_back(HPS::Point(vFromPos[0], vFromPos[1], vFromPos[2]));
			else
				points.push_back(HPS::Point(vToPos[0], vToPos[1], vToPos[2]));
			hpsfaces.push_back(pointIndex);
			pointIndex++;
		}

		aEntity = PFEntityListGetNext(face_list, aEntity);
	}

	shellKit.SetFacelist(hpsfaces);
	shellKit.SetPoints(points);

	return status;
}

void CombineShells(HPS::Key key)
{
	if (key.Type() == HPS::Type::SegmentKey)
	{
		HPS::SegmentKey segKey(key);
		HPS::SearchResults searchResults;

		size_t numResults = segKey.Find(HPS::Search::Type::Shell, HPS::Search::Space::SubsegmentsAndIncludes, searchResults);

		HPS::SearchResultsIterator it = searchResults.GetIterator();

		size_t vertexCount = 0;
		size_t fListCount = 0;

		HPS::PointArray newPointArray = HPS::PointArray();
		HPS::IntArray newfListArray = HPS::IntArray();

		while (it.IsValid())
		{
			HPS::Key key = it.GetItem();

			if (key.Type() == HPS::Type::ShellKey)
			{
				HPS::PointArray points;
				HPS::IntArray flist;

				HPS::ShellKey sKey(key);

				sKey.ShowPoints(points);
				newPointArray.insert(newPointArray.end(), points.begin(), points.end());

				sKey.ShowFacelist(flist);

				auto i = 0;
				while (i < flist.size())
				{
					//ASSERT(flist[i] = 3);

					flist[i + 1] += vertexCount;
					flist[i + 2] += vertexCount;
					flist[i + 3] += vertexCount;

					i += 4;
				}

				newfListArray.insert(newfListArray.end(), flist.begin(), flist.end());

				vertexCount += points.size();
			}
			it.Next();
		}

		segKey.Flush(HPS::Search::Type::Geometry);

		HPS::ShellKey shellKey = segKey.InsertShell(newPointArray, newfListArray);

		HPS::ShellOptimizationOptionsKit sook;
		sook.SetTolerance(0.01f, HPS::Shell::ToleranceUnits::FeatureSizePercentage);
		sook.SetNormalTolerance(180.0f);
		sook.SetOrphanElimination(true);
		sook.SetHandednessOptimization(HPS::Shell::HandednessOptimization::None);
		shellKey.Optimize(sook);

		/*HPS::SegmentOptimizationOptionsKit segmentOptimizationOptions;
		segmentOptimizationOptions.SetShellMerging(true);

		segmentOptimizationOptions.SetUserData(HPS::SegmentOptimizationOptions::UserData::Discard);
		segmentOptimizationOptions.SetScope(HPS::SegmentOptimizationOptions::Scope::SubsegmentsAndIncludes);
		segmentOptimizationOptions.SetExpansion(HPS::SegmentOptimizationOptions::Expansion::IncludesAndReferences);
		segmentOptimizationOptions.SetReorganization(HPS::SegmentOptimizationOptions::Reorganization::Attribute);
		segKey.Optimize(segmentOptimizationOptions);*/
	}
}

static PTPointer get_indexed_lattice_vertices(PTPointer app_data,
	PTNat32 vertex_num,
	PTNat32 *num_vertices)

{
	IndexedLattice *indexed_lattice;
	PTPoint *vertices;

	indexed_lattice = (IndexedLattice *)app_data;

	vertices = &indexed_lattice->vertices[vertex_num];
	*num_vertices = indexed_lattice->num_vertices - vertex_num;

	return vertices;
}

static PTPointer get_indexed_lattice_indices(PTPointer app_data,
	PTNat32 segment_num,
	PTNat32 *num_segments)

{
	IndexedLattice *indexed_lattice;
	PTNat32 *indices;

	indexed_lattice = (IndexedLattice *)app_data;

	indices = &indexed_lattice->segment_indices[2 * segment_num];
	*num_segments = indexed_lattice->num_segments - segment_num;

	return indices;
}

static PTBoolean create_render_lattice(PTEnvironment env,
	PTLattice lattice,
	HPS::SegmentKey latticeSeg,
	float radius)
{
	PTNat32 num_segments;
	PTNat32 num_vertices;
	PTPoint *vertices;
	PTNat32 *indices;
	PTCurve curve;
	PTNat32 i;
	PTWorldEntity curve_world_entity;

	num_segments = PFEntityGetNat32Property(lattice, PV_LATTICE_PROP_NUM_SEGMENTS);
	num_vertices = PFEntityGetNat32Property(lattice, PV_LATTICE_PROP_NUM_VERTICES);

	if (num_vertices == 0)
		return FALSE;

	/* Allocate some memory to hold the lattice data */
	if (!(vertices = (PTPoint*)malloc(num_vertices * sizeof(PTPoint))))
		return FALSE;
	if (!(indices = (PTNat32*)malloc(2 * num_segments * sizeof(PTNat32))))
		return FALSE;

	/* Query the lattice data */
	if (PFLatticeQueryDirectIndexed(lattice,
		PV_LATTICE_QUERY_VERTEX_COORDS,
		PV_DATA_TYPE_DOUBLE, vertices,
		0, num_vertices) != PV_STATUS_OK)
		return FALSE;

	if (PFLatticeQueryDirectIndexed(lattice,
		PV_LATTICE_QUERY_SEG_INDICES,
		PV_DATA_TYPE_UNSIGNED_INT32,
		indices, 0, num_segments) != PV_STATUS_OK)
		return FALSE;

	for (i = 0; i < num_segments; ++i)
	{
		HPS::PointArray points;
		HPS::FloatArray radii;
		points.push_back(HPS::Point(vertices[indices[2 * i]][0], vertices[indices[2 * i]][1], vertices[indices[2 * i]][2]));
		radii.push_back(radius);
		points.push_back(HPS::Point(vertices[indices[2 * i + 1]][0], vertices[indices[2 * i + 1]][1], vertices[indices[2 * i + 1]][2]));
		radii.push_back(radius);
		if( !((points[0].x == points[1].x) && (points[0].y == points[1].y) && (points[0].z == points[0].z))) // avoids crash (bug in hps)
			latticeSeg.InsertCylinder(points, radii);
	}

	free(vertices);
	free(indices);

	return TRUE;
}

bool GenerateLatticeInternal(LatticeParams *latticeParams)
{
	PTStatus status = PFThreadRegister();

	//LatticeParams *latticeParams = (LatticeParams*)pCUPDUPData->GetAppData();

	PTLattice lattice, clipped_lattice;

	PTLatticeCreateDirectOpts create_lattice_opts;
	PMInitLatticeCreateDirectOpts(&create_lattice_opts);

	IndexedLattice indexed_lattice;

	float step = latticeParams->fStep;

	latticeParams->direction = axis::Y;

	switch (latticeParams->direction)
	{
	case axis::X:
	{
		// Create and individual cell
		PTPoint cellPoints[5] = {
			{ 0, 0, 0 },
			{ 1 * step, 0, 0},
			{ 1.5 * step, 0,  .886 * step},
			{ 1.5 * step, -.75 * step, -.433 * step},
			{ 1.5 * step , .75 * step, -.433 * step}
		};

		PTNat32 cellSegments[8] = { 0, 1, 1, 2, 1, 3, 1, 4 };

		float zmin = latticeParams->min[2] - 1.299*step; // is this right? shouldn't it be ymin?
		int xsize = ceil((latticeParams->max[0] - latticeParams->min[0]) / (step*1.5));
		int ysize = ceil((latticeParams->max[1] - latticeParams->min[1]) / (step*1.5));
		int zsize = ceil((latticeParams->max[2] - zmin) / (step*1.299));

		indexed_lattice.vertices = (PTPoint*)malloc(sizeof(PTPoint) * 5 * xsize * ysize * zsize);
		indexed_lattice.segment_indices = (PTNat32 *)malloc(8 * sizeof(PTNat32) * xsize * ysize * zsize);
		indexed_lattice.num_vertices = 5 * xsize * ysize * zsize;
		indexed_lattice.num_segments = 4 * xsize * ysize * zsize;


		for (int z = 0; z < zsize; z++)
		{
			for (int y = 0; y < ysize; y++)
			{
				for (int x = 0; x < xsize; x++)
				{
					int v = 5 * (z*xsize*ysize + y*xsize + x);

					for (int i = 0; i < 5; i++)
					{
						indexed_lattice.vertices[v + i][0] = cellPoints[i][0] + x * 1.5 * step + latticeParams->min[0]; //xmin
						indexed_lattice.vertices[v + i][1] = cellPoints[i][1] + y * 1.5 * step + (z % 2)*.75 * step + latticeParams->min[1];
						indexed_lattice.vertices[v + i][2] = cellPoints[i][2] + z * 1.299 * step + (x % 3)*.866 * step + zmin;
					}

					int s = 8 * (z*xsize*ysize + y*xsize + x);

					for (int i = 0; i < 8; i++)
					{
						indexed_lattice.segment_indices[s + i] = cellSegments[i] + v;
					}
				}
			}
		}
		break;
	}
	case axis::Y:
	{
		// Create and individual cell
		PTPoint cellPoints[5] = {
			{ 0, 0, 0 },
			{ 0, 1 * step, 0 },
			{ 0, 1.5 * step, .886 * step },
			{ -.75 * step, 1.5 * step, -.433 * step},
			{ .75 * step, 1.5 * step, -.433 * step}
		};

		PTNat32 cellSegments[8] = { 0, 1, 1, 2, 1, 3, 1, 4 };

		float zmin = latticeParams->min[2] - 1.299*step;
		int xsize = ceil((latticeParams->max[0] - latticeParams->min[0]) / (step*1.5));
		int ysize = ceil((latticeParams->max[1] - latticeParams->min[1]) / (step*1.5));
		int zsize = ceil((latticeParams->max[2] - zmin) / (step*1.299));

		indexed_lattice.vertices = (PTPoint*)malloc(sizeof(PTPoint) * 5 * xsize * ysize * zsize);
		indexed_lattice.segment_indices = (PTNat32 *)malloc(8 * sizeof(PTNat32) * xsize * ysize * zsize);
		indexed_lattice.num_vertices = 5 * xsize * ysize * zsize;
		indexed_lattice.num_segments = 4 * xsize * ysize * zsize;


		for (int z = 0; z < zsize; z++)
		{
			for (int y = 0; y < ysize; y++)
			{
				for (int x = 0; x < xsize; x++)
				{
					int v = 5 * (z*xsize*ysize + y*xsize + x);

					for (int i = 0; i < 5; i++)
					{
						indexed_lattice.vertices[v + i][0] = cellPoints[i][0] + x * 1.5 * step + (z % 2)*.75 * step + latticeParams->min[0];// xmin;
						indexed_lattice.vertices[v + i][1] = cellPoints[i][1] + y * 1.5 * step + latticeParams->min[1];
						indexed_lattice.vertices[v + i][2] = cellPoints[i][2] + z * 1.299 * step + (y % 3)*.866 * step + zmin;
					}

					int s = 8 * (z*xsize*ysize + y*xsize + x);

					for (int i = 0; i < 8; i++)
					{
						indexed_lattice.segment_indices[s + i] = cellSegments[i] + v;
					}
				}
			}
		}
		break;
	}
	case axis::Z:
	{
		// Create and individual cell
		PTPoint cellPoints[5] = {
			{0, 0, 0},
			{0, 0, 1 * step },
			{.886 * step, 0, 1.5 * step },
			{-.433 * step, -.75 * step, 1.5 * step },
			{-.433 * step, .75 * step, 1.5 * step }
		};

		PTNat32 cellSegments[8] = { 0, 1, 1, 2, 1, 3, 1, 4 };

		float xmin = latticeParams->min[0] - 1.299*step;
		int xsize = ceil((latticeParams->max[0] - xmin) / (step*1.299));
		int ysize = ceil((latticeParams->max[1] - latticeParams->min[1]) / (step*1.5));
		int zsize = ceil((latticeParams->max[2] - latticeParams->min[2]) / (step*1.5));

		indexed_lattice.vertices = (PTPoint*)malloc(sizeof(PTPoint) * 5 * xsize * ysize * zsize);
		indexed_lattice.segment_indices = (PTNat32 *)malloc(8 * sizeof(PTNat32) * xsize * ysize * zsize);
		indexed_lattice.num_vertices = 5 * xsize * ysize * zsize;
		indexed_lattice.num_segments = 4 * xsize * ysize * zsize;


		for (int z = 0; z < zsize; z++)
		{
			for (int y = 0; y < ysize; y++)
			{
				for (int x = 0; x < xsize; x++)
				{
					int v = 5 * (z*xsize*ysize + y*xsize + x);

					for (int i = 0; i < 5; i++)
					{
						indexed_lattice.vertices[v + i][0] = cellPoints[i][0] + x * 1.299 * step + (z % 3)*.866 * step + xmin;
						indexed_lattice.vertices[v + i][1] = cellPoints[i][1] + y * 1.5 * step + (x % 2)*.75 * step + latticeParams->min[1];
						indexed_lattice.vertices[v + i][2] = cellPoints[i][2] + z * 1.5 * step + latticeParams->min[2];
					}

					int s = 8 * (z*xsize*ysize + y*xsize + x);

					for (int i = 0; i < 8; i++)
					{
						indexed_lattice.segment_indices[s + i] = cellSegments[i] + v;
					}
				}
			}
		}
		break;
	}
	}

	/* Allocate sufficient storage to hold the vertex coordinates and segment indices * /
	int grid_size = (int)(ceil((latticeParams->max[0] - latticeParams->min[0]) / latticeParams->fStep) *  ceil((latticeParams->max[1] - latticeParams->min[1]) / latticeParams->fStep)) +
		(int)(ceil((latticeParams->max[0] - latticeParams->min[0]) / latticeParams->fStep) *  ceil((latticeParams->max[2] - latticeParams->min[2]) / latticeParams->fStep)) +
		(int)(ceil((latticeParams->max[1] - latticeParams->min[1]) / latticeParams->fStep) *  ceil((latticeParams->max[2] - latticeParams->min[2]) / latticeParams->fStep));
	if (!(indexed_lattice.vertices = (PTPoint*)malloc(grid_size * 2 * sizeof(PTPoint))) ||
		!(indexed_lattice.segment_indices = (PTNat32 *)malloc(grid_size * 2 * sizeof(PTNat32))))
	{
		::AfxMessageBox(L"Failed to create indexed lattice");
		free(indexed_lattice.vertices);
		free(indexed_lattice.segment_indices);
		status = PFThreadUnregister();
		return false;
	}

	float x, y, z;
	double *p;
	int vertex_index = 0;
	int segment_index = 0;

	for (x = latticeParams->min[0]; x < latticeParams->max[0]; x += latticeParams->fStep)
		for (y = latticeParams->min[1]; y < latticeParams->max[1]; y += latticeParams->fStep)
		{
			indexed_lattice.segment_indices[segment_index++] = vertex_index;
			p = indexed_lattice.vertices[vertex_index++];
			p[0] = x;
			p[1] = y;
			p[2] = latticeParams->min[2];

			indexed_lattice.segment_indices[segment_index++] = vertex_index;
			p = indexed_lattice.vertices[vertex_index++];
			p[0] = x;
			p[1] = y;
			p[2] = latticeParams->max[2];
		}

	for (x = latticeParams->min[0]; x < latticeParams->max[0]; x += latticeParams->fStep)
		for (z = latticeParams->min[2]; z < latticeParams->max[2]; z += latticeParams->fStep)
		{
			indexed_lattice.segment_indices[segment_index++] = vertex_index;
			p = indexed_lattice.vertices[vertex_index++];
			p[0] = x;
			p[1] = latticeParams->min[1];
			p[2] = z;

			indexed_lattice.segment_indices[segment_index++] = vertex_index;
			p = indexed_lattice.vertices[vertex_index++];
			p[0] = x;
			p[1] = latticeParams->max[1];
			p[2] = z;
		}

	for (y = latticeParams->min[1]; y < latticeParams->max[1]; y += latticeParams->fStep)
		for (z = latticeParams->min[2]; z < latticeParams->max[2]; z += latticeParams->fStep)
		{
			indexed_lattice.segment_indices[segment_index++] = vertex_index;
			p = indexed_lattice.vertices[vertex_index++];
			p[0] = latticeParams->min[0];
			p[1] = y;
			p[2] = z;

			indexed_lattice.segment_indices[segment_index++] = vertex_index;
			p = indexed_lattice.vertices[vertex_index++];
			p[0] = latticeParams->max[0];
			p[1] = y;
			p[2] = z;
		}

	indexed_lattice.num_vertices = vertex_index;
	indexed_lattice.num_segments = segment_index / 2;

	// Create the lattice
	/*status = PFLatticeCreateDirectIndexed(latticeParams->pg_environment,
		indexed_lattice.num_vertices,
		indexed_lattice.num_segments,
		&indexed_lattice,
		get_indexed_lattice_vertices,
		get_indexed_lattice_indices,
		&create_lattice_opts,
		&lattice);

	status = PFLatticeClipToSolid(lattice, latticeParams->solid,
		NULL,
		&latticeParams->lattice);*/

	status = PFLatticeCreateDirectIndexed(latticeParams->pg_environment,
		indexed_lattice.num_vertices,
		indexed_lattice.num_segments,
		&indexed_lattice,
		get_indexed_lattice_vertices,
		get_indexed_lattice_indices,
		&create_lattice_opts,
		&lattice);

	/*status = PFLatticeClipToSolid(lattice, latticeParams->solid,
		NULL,
		&latticeParams->lattice);*/

	status = PFSolidCreateClippedLattice(latticeParams->solid, lattice,
		NULL,
		&latticeParams->lattice);

	free(indexed_lattice.vertices);
	free(indexed_lattice.segment_indices);

	status = PFThreadUnregister();

	return true;
}

bool GenerateLattice(HPS::SegmentKey root, std::unordered_map<intptr_t,
	PgMapItem> pg_map, 
	const HWND hwnd, 
	PTEnvironment environment, PTSolid *inner_solid_array,
	HPS::UTF8 units)
{
	HPS::SimpleSphere sphere;
	HPS::SimpleCuboid cube;
	root.GetBoundingControl().ShowVolume(sphere, cube);

	/*pgLatticeDialog dlg;
	dlg.m_sDimensions.Format(L"Bounding dimensions: %.1f x %.1f x %.1f %s",
		cube.max.x - cube.min.x,
		cube.max.y - cube.min.y,
		cube.max.z - cube.min.z,
		GetUnitsAbreviation(units));

	dlg.m_fSpacing = sphere.radius * 0.05;

	if (dlg.DoModal() != IDOK)
		return false;*/

	float step = 0.01;// dlg.m_fSpacing;

	std::unordered_map<intptr_t, PgMapItem>::iterator it = pg_map.begin();

	int i = 0;

	while (it != pg_map.end())
	{
		PgMapItem mapItem = it->second;
		PTSolid target = mapItem.solid;
		HPS::ShellKey shell = mapItem.shellKey;

		LatticeParams latticeParams;
		latticeParams.fStep = step; // dlg.m_fSpacing;
		latticeParams.solid = inner_solid_array[i++]; // this is going to be tough, we need to create one for each shape.
		latticeParams.pg_environment = environment;
		latticeParams.direction = axis::X; // dlg.m_Axis;

		// Get the bounds just for the shell that we are working on
		HPS::BoundingKit bounds;
		shell.ShowBounding(bounds);
		bounds.ShowVolume(sphere, cube);
		latticeParams.min[0] = cube.min.x;
		latticeParams.min[1] = cube.min.y;
		latticeParams.min[2] = cube.min.z;
		latticeParams.max[0] = cube.max.x;
		latticeParams.max[1] = cube.max.y;
		latticeParams.max[2] = cube.max.z;

		/*const CUPDUPDATA* pCUPDUPData;
		CUPDialog dlg2(hwnd, GenerateLatticeInternal, &latticeParams);
		INT_PTR nResult = dlg2.DoModal();*/

		HPS::SegmentKey latticeSeg = shell.Owner().Subsegment("lattice");
		latticeSeg.Flush(HPS::Search::Type::Geometry);

		latticeSeg.GetVisibilityControl().SetLines(true);
		latticeSeg.GetMaterialMappingControl().SetFaceColor(HPS::RGBColor(.2, .2, .2));
		latticeSeg.GetLineAttributeControl().SetWeight(4);
		HPS::AttributeLockTypeArray lockTypeArray;
		lockTypeArray.push_back(HPS::AttributeLock::Type::MaterialFace);
		HPS::BoolArray lockStatus(1, true);
		latticeSeg.GetAttributeLockControl().SetLock(lockTypeArray, lockStatus);

		create_render_lattice(environment, latticeParams.lattice/* clipped_lattice*/, latticeSeg, latticeParams.fStep / 10.0f);// world, curve_style, &curve))

		it++;
	}

	return true;
}