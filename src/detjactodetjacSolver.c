

#include "detjactodetjacSolver.h"



//the main wrapper function for chaining into the detjac_to_detjac solver.
// pass in:
//     •
//
// you will get out: W_new populated in both mp and double types, ...
//TODO: FINISH THIS EXPLANATION


int detjac_to_detjac_solver_main(int MPType,
																 witness_set_d W, // carries with it the start points, and the linears.
																 mat_mp n_minusone_randomizer_matrix_full_prec,
																 vec_mp old_projection_full_prec,
																 vec_mp new_projection_full_prec,
																 witness_set_d *W_new)
{
	W_new->num_variables = W.num_variables;
	
	cp_patches(W_new,W); // copy the patches over from the original witness set.  for completeness
	
	W_new->num_linears = (1);
	W_new->L = (vec_d *)bmalloc((1)*sizeof(vec_d)); init_vec_d(W_new->L[0],W.num_variables);
	W_new->L_mp = (vec_mp *)bmalloc((1)*sizeof(vec_mp)); init_vec_mp(W_new->L_mp[0],W.num_variables);
	
	
	vec_cp_mp(W_new->L_mp[0],new_projection_full_prec);
	vec_mp_to_d(W_new->L[0],new_projection_full_prec);
	
	
	if (MPType==1){
		detjac_to_detjac_solver_mp(MPType,W,n_minusone_randomizer_matrix_full_prec,old_projection_full_prec,new_projection_full_prec, W_new);
	}
	else{
		detjac_to_detjac_solver_d( MPType,W,n_minusone_randomizer_matrix_full_prec,old_projection_full_prec,new_projection_full_prec,W_new);
	}
	
	
	return 0;
}




//int detjac_to_detjac_solver_d(int MPType,
//															 witness_set_d W,  // includes the initial linear.
//															 mat_d n_minusone_randomizer_matrix,  // for randomizing down to N-1 equations.
//															 vec_d projection,
//															 witness_set_d *W_new // for passing the data back out of this function tree
//															 )


int detjac_to_detjac_solver_d(int MPType, //, double parse_time, unsigned int currentSeed
															witness_set_d W,  // includes the initial linear.
															mat_mp n_minusone_randomizer_matrix_full_prec,  // for randomizing down to N-1 equations.
															vec_mp old_projection_full_prec,   // collection of random complex linears.  for setting up the regeneration for V(f\\g)
															vec_mp new_projection_full_prec,
															
															witness_set_d *W_new)
/***************************************************************\
 * USAGE:                                                        *
 * ARGUMENTS:                                                    *
 * RETURN VALUES:                                                *
 * NOTES:                                                        *
 \***************************************************************/
{
	double parse_time = 0;
  FILE *OUT = NULL, *FAIL = safe_fopen_write("failed_paths"), *midOUT = NULL, *rawOUT = safe_fopen_write("raw_data");
  tracker_config_t T;
  prog_t dummyProg;
  bclock_t time1, time2;
  int num_variables = 0, convergence_failures = 0, sharpening_failures = 0, sharpening_singular = 0, num_crossings = 0, num_sols = 0;
	
	//necessary for the setupConfig call
	double midpoint_tol, intrinsicCutoffMultiplier;
	int userHom = 0, useRegen = 0, regenStartLevel = 0, maxCodim = 0, specificCodim = 0, pathMod = 0, reducedOnly = 0, supersetOnly = 0, paramHom = 0;
	//end necessaries for the setupConfig call.
	
  int usedEq = 0;
  int *startSub = NULL, *endSub = NULL, *startFunc = NULL, *endFunc = NULL, *startJvsub = NULL, *endJvsub = NULL, *startJv = NULL, *endJv = NULL, **subFuncsBelow = NULL;
  int (*ptr_to_eval_d)(point_d, point_d, vec_d, mat_d, mat_d, point_d, comp_d, void const *) = NULL;
  int (*ptr_to_eval_mp)(point_mp, point_mp, vec_mp, mat_mp, mat_mp, point_mp, comp_mp, void const *) = NULL;
	
  detjactodetjac_eval_data_d ED;
  trackingStats trackCount;
  char inputName[] = "func_input";
  double track_time;
	
  bclock(&time1); // initialize the clock.
  init_trackingStats(&trackCount); // initialize trackCount to all 0
	
  // setup T
  setupConfig(&T, &midpoint_tol, &userHom, &useRegen, &regenStartLevel, &maxCodim, &specificCodim, &pathMod, &intrinsicCutoffMultiplier, &reducedOnly, &supersetOnly, &paramHom, MPType);
	
	
	
	//  // call the setup function
	// setup for standard tracking - 'useRegen' is used to determine whether or not to setup 'start'
	num_variables = detjac_to_detjac_setup_d(&OUT, "output",
																					 &midOUT, "midpath_data",
																					 &T, &ED,
																					 &dummyProg,  //arg 7
																					 &startSub, &endSub, &startFunc, &endFunc,
																					 &startJvsub, &endJvsub, &startJv, &endJv, &subFuncsBelow,
																					 &ptr_to_eval_d, &ptr_to_eval_mp,  //args 17,18
																					 "preproc_data", "deg.out",
																					 !useRegen, "nonhom_start", "start",
																					 n_minusone_randomizer_matrix_full_prec,W,
																					 old_projection_full_prec,
																					 new_projection_full_prec);
  
	int (*change_prec)(void const *, int) = NULL;
	change_prec = &change_detjactodetjac_eval_prec;
	
	int (*dehom)(point_d, point_mp, int *, point_d, point_mp, int, void const *, void const *) = NULL;
	dehom = &detjactodetjac_dehom;
	
	
	
  // error checking
  if (userHom <= 0 && paramHom != 2)
  { // no pathvariables or parameters allowed!
    if (dummyProg.numPathVars > 0)
    { // path variable present
      printf("ERROR: Bertini does not expect path variables when user-defined homotopies are not being used!\n");
      bexit(ERROR_INPUT_SYSTEM);
    }
    if (dummyProg.numPars > 0)
    { // parameter present
      printf("ERROR: Bertini does not expect parameters when user-defined homotopies are not being used!\n");
      bexit(ERROR_INPUT_SYSTEM);
    }
  }
	
  if (T.MPType == 2)  //If we are doing adaptive precision path-tracking, we must set up AMP_eps, AMP_Phi, AMP_Psi based on config settings.
  {
    T.AMP_eps = (double) num_variables * num_variables;  //According to Demmel (as in the AMP paper), n^2 is a very reasonable bound for \epsilon.
    T.AMP_Phi = T.AMP_bound_on_degree*(T.AMP_bound_on_degree-1.0)*T.AMP_bound_on_abs_vals_of_coeffs;  //Phi from the AMP paper.
    T.AMP_Psi = T.AMP_bound_on_degree*T.AMP_bound_on_abs_vals_of_coeffs;  //Psi from the AMP paper.
    // initialize latest_newton_residual_mp to the maximum precision
    mpf_init2(T.latest_newton_residual_mp, T.AMP_max_prec);
  }
	
	
	
	
	if (T.endgameNumber == 3)
	{ // use the track-back endgame
		//        zero_dim_trackBack_d(&trackCount, OUT, rawOUT, midOUT, StartPts, FAIL, pathMod, &T, &ED, ED.BED_mp, ptr_to_eval_d, ptr_to_eval_mp, change_basic_eval_prec, zero_dim_dehom);
		printf("bertini_real not equipped to deal with endgameNumber 3\nexiting\n");
		exit(-99);
	}
	else
	{ // use regular endgame
		detjac_to_detjac_track_d(&trackCount, OUT, rawOUT, midOUT,
														 W,  // was the startpts file pointer.
														 W_new,
														 FAIL, pathMod,
														 &T, &ED, ED.BED_mp,
														 ptr_to_eval_d, ptr_to_eval_mp,
														 change_prec, dehom);
	}
	
	
	
	
	fclose(midOUT);
	
	
	
	
	// finish the output to rawOUT
	fprintf(rawOUT, "%d\n\n", -1);  // bottom of rawOUT
	
	
	// check for path crossings
	midpoint_checker(trackCount.numPoints, num_variables, midpoint_tol, &num_crossings);
	
	// setup num_sols
	num_sols = trackCount.successes;
	
  // we report how we did with all paths:
  bclock(&time2);
  totalTime(&track_time, time1, time2);
  if (T.screenOut)
  {
    printf("Number of failures:  %d\n", trackCount.failures);
    printf("Number of successes:  %d\n", trackCount.successes);
    printf("Number of paths:  %d\n", trackCount.numPoints);
    printf("Parse Time = %fs\n", parse_time);
    printf("Track Time = %fs\n", track_time);
  }
  fprintf(OUT, "Number of failures:  %d\n", trackCount.failures);
  fprintf(OUT, "Number of successes:  %d\n", trackCount.successes);
  fprintf(OUT, "Number of paths:  %d\n", trackCount.numPoints);
  fprintf(OUT, "Parse Time = %fs\n", parse_time);
  fprintf(OUT, "Track Time = %fs\n", track_time);
	
  // print the system to rawOUT
	//  printdetjactodetjacRelevantData(&ED, ED.BED_mp, T.MPType, usedEq, rawOUT);
	//	printZeroDimRelevantData(&ED, ED.BED_mp, T.MPType, usedEq, rawOUT);// legacy
	
  // close all of the files
  fclose(OUT);
  fclose(rawOUT);
  fprintf(FAIL, "\n");
  fclose(FAIL);
	
  // reproduce the input file needed for this run
	//  reproduceInputFile(inputName, "func_input", &T, 0, 0, currentSeed, pathMod, userHom, useRegen, regenStartLevel, maxCodim, specificCodim, intrinsicCutoffMultiplier, reducedOnly, supersetOnly, paramHom);
	
	//  // print the output
	
	
  // do the standard post-processing
	//  sort_points(num_crossings, &convergence_failures, &sharpening_failures, &sharpening_singular, inputName, num_sols, num_variables, midpoint_tol, T.final_tol_times_mult, &T, &ED.preProcData, useRegen == 1 && userHom == 0, userHom == -59);
	
	
	
  // print the failure summary
	//  printFailureSummary(&trackCount, convergence_failures, sharpening_failures, sharpening_singular);
	
	
	
	//DAB is there other stuff which should be cleared here?
	
	free(startSub);
	free(endSub);
	free(startFunc);
	free(endFunc);
	free(startJvsub);
	free(endJvsub);
	free(startJv);
	free(endJv);
	
	
	
  detjactodetjac_eval_clear_d(&ED, userHom, T.MPType);
  tracker_config_clear(&T);
	
  return 0;
}





void detjac_to_detjac_track_d(trackingStats *trackCount,
															FILE *OUT, FILE *RAWOUT, FILE *MIDOUT,
															witness_set_d W,
															witness_set_d *W_new,  // for holding the produced data.
															FILE *FAIL,
															int pathMod, tracker_config_t *T,
															detjactodetjac_eval_data_d *ED_d,
															detjactodetjac_eval_data_mp *ED_mp,
															int (*eval_func_d)(point_d, point_d, vec_d, mat_d, mat_d, point_d, comp_d, void const *),
															int (*eval_func_mp)(point_mp, point_mp, vec_mp, mat_mp, mat_mp, point_mp, comp_mp, void const *),
															int (*change_prec)(void const *, int),
															int (*find_dehom)(point_d, point_mp, int *, point_d, point_mp, int, void const *, void const *))
/***************************************************************\
 * USAGE:                                                        *
 * ARGUMENTS:                                                    *
 * RETURN VALUES:                                                *
 * NOTES: does standard zero dimensional tracking                *
 *  in either double precision or adaptive precision             *
 \***************************************************************/
{
	
  int ii,jj,kk, oid, startPointIndex, max = max_threads();
  tracker_config_t *T_copy = NULL;
  detjactodetjac_eval_data_d *BED_copy = NULL;
  trackingStats *trackCount_copy = NULL;
  FILE **OUT_copy = NULL, **MIDOUT_copy = NULL, **RAWOUT_copy = NULL, **FAIL_copy = NULL, *NONSOLN = NULL, **NONSOLN_copy = NULL;
	
  // top of RAWOUT - number of variables and that we are doing zero dimensional
  fprintf(RAWOUT, "%d\n%d\n", T->numVars, 0);
	
	
	
	
	int (*curr_eval_d)(point_d, point_d, vec_d, mat_d, mat_d, point_d, comp_d, void const *) = NULL;
  int (*curr_eval_mp)(point_mp, point_mp, vec_mp, mat_mp, mat_mp, point_mp, comp_mp, void const *) = NULL;
	
	curr_eval_d = &detjac_to_detjac_eval_d;   //DAB
  curr_eval_mp = &detjac_to_detjac_eval_mp; // DAB
	
	
	
	point_data_d *startPts = NULL;
	startPts = (point_data_d *)bmalloc(W.W.num_pts * sizeof(point_data_d));
	
	
	
	for (ii = 0; ii < W.W.num_pts; ii++)
	{ // setup startPts[ii]
		init_point_data_d(&startPts[ii], W.num_variables); // also performs initialization on the point inside startPts
		change_size_vec_d(startPts[ii].point,W.num_variables);
		startPts[ii].point->size = W.num_variables;
		
		//NEED TO COPY IN THE WITNESS POINT
		
		//1 set the coordinates
		for (jj = 0; jj<W.num_variables; jj++) {
			startPts[ii].point->coord[jj].r = W.W.pts[ii]->coord[jj].r;
			startPts[ii].point->coord[jj].i = W.W.pts[ii]->coord[jj].i;
		}
		//2 set the start time to 1.
		set_one_d(startPts[ii].time);
	}
	T->endgameOnly = 0;
	
	
  // setup the rest of the structures
	endgame_data_t *EG = NULL; //this will hold the temp solution data produced for each individual track
  setup_detjac_to_detjac_omp_d(max,
															 &EG, &trackCount_copy, trackCount,
															 &OUT_copy, OUT, &RAWOUT_copy, RAWOUT, &MIDOUT_copy, MIDOUT, &FAIL_copy, FAIL, &NONSOLN_copy, NONSOLN,
															 &T_copy, T,
															 &BED_copy, ED_d, ED_mp);
	
	
	//initialize the structure for holding the produced data

	
	W_new->W.num_pts = 0;
  W_new->W.pts=(point_d *)bmalloc(W.W.num_pts*sizeof(point_d));
	
	W_new->W_mp.num_pts = 0;
  W_new->W_mp.pts=(point_mp *)bmalloc(W.W.num_pts*sizeof(point_mp));

	

	
	
	trackCount->numPoints = W.W.num_pts;
	int solution_counter = 0;
	
	
	//	for (kk = 0; kk< num_new_linears; kk++)
	//	{
	//#ifdef verbose
	//		printf("solving for linear %d\n",kk);
	//#endif
	//		// we pass the particulars of the information for this solve mode via the ED.
	//
	//		//set current linear in the evaluator data's
	//		vec_mp_to_d(     ED_d->current_linear,new_linears_full_prec[kk]);
	//
	//		if (T->MPType==2) {
	//			vec_cp_mp(ED_d->BED_mp->current_linear,new_linears_full_prec[kk]);
	//			vec_cp_mp(ED_d->BED_mp->current_linear_full_prec,new_linears_full_prec[kk]);
	//			//			point_d_to_mp(ED_d->BED_mp->current_linear,ED_d->current_linear);
	//		}
	//
	//		//copy the linears into the new witness_set
	//		init_vec_d(W_new->L[kk],0); init_vec_mp(W_new->L_mp[kk],0);
	//		vec_mp_to_d(   W_new->L[kk],new_linears_full_prec[kk]);
	//		vec_cp_mp(W_new->L_mp[kk],new_linears_full_prec[kk]);
	//
	//
	
	// track each of the start points
#ifdef _OPENMP
#pragma omp parallel for private(ii, oid, startPointIndex) schedule(runtime)
#endif
	for (ii = 0; ii < W.W.num_pts; ii++)
	{ // get current thread number
		oid = thread_num();
		
		printf("detjac_to_detjac tracking path %d of %d\n",ii,W.W.num_pts);
		startPointIndex = ii;
		
		
		// print the header of the path to OUT
		printPathHeader_d(OUT_copy[oid], &startPts[startPointIndex], &T_copy[oid], ii, &BED_copy[oid], eval_func_d);
		
		// track the path
		detjac_to_detjac_track_path_d(solution_counter, &EG[oid], &startPts[startPointIndex], OUT_copy[oid], MIDOUT_copy[oid], &T_copy[oid], &BED_copy[oid], BED_copy[oid].BED_mp, curr_eval_d, curr_eval_mp, change_prec, find_dehom);
		
		
		
		// check to see if it should be sharpened
		if (EG[oid].retVal == 0 && T_copy[oid].sharpenDigits > 0)
		{ // use the sharpener for after an endgame
			sharpen_endpoint_endgame(&EG[oid], &T_copy[oid], OUT_copy[oid], &BED_copy[oid], BED_copy[oid].BED_mp, curr_eval_d, curr_eval_mp, change_prec);
		}
		
		
		init_vec_d(W_new->W.pts[solution_counter],0);
		init_vec_mp(W_new->W_mp.pts[solution_counter],0);
		
		change_size_vec_d(W_new->W.pts[solution_counter],W.num_variables);
		W_new->W.pts[solution_counter]->size = W.num_variables;
		
		
		
		
		if (EG->retVal!=0) {
			printf("\nretVal = %d\nthere was a path failure tracking witness point %d in detjac_to_detjac\n\n",EG->retVal,ii);
			print_path_retVal_message(EG->retVal);
			
			//			exit(EG->retVal);
		}
		else
		{
			
			//copy the solutions out of EG.
			
			if (T->MPType==2) {
				printf("%d\n",EG->prec);
				if (EG->prec<64) {
					//					printf("copying from double?\n");
					vec_d_to_mp(W_new->W_mp.pts[solution_counter],EG->PD_d.point);
				}
				else{
					//					printf("copying from mp?\n");
					vec_cp_mp(W_new->W_mp.pts[solution_counter],EG->PD_mp.point);
				}
				
				vec_mp_to_d(W_new->W.pts[solution_counter],W_new->W_mp.pts[solution_counter]);
				// verification, if needed
				//				comp_mp result;
				//				init_mp(result);
				//				dot_product_mp(result, W_new->W_mp.pts[solution_counter], ED_mp->current_linear);
				//				print_comp_mp_matlab(result,"res");
			}
			else{ // MPType == 0
				vec_cp_d( W_new->W.pts[solution_counter], EG->PD_d.point);
				vec_d_to_mp(W_new->W_mp.pts[solution_counter], EG->PD_d.point);
				
				// verification, if needed.
				//				comp_d result;
				//				dot_product_d(result, W_new->W.pts[solution_counter], ED_d->current_linear);
				//				printf("dot_prod=%le+1i*%le;\n",result->r,result->i);
			}
			
			//			mypause();
			solution_counter++;
		}
		
		
		
		
		
		
		//				print_point_to_screen_matlab(startPts[startPointIndex].point,"start");
		//				print_point_to_screen_matlab(ED_d->old_linear,"old");
		//
		//				print_point_to_screen_matlab(EG->PD_d.point,"vend"); // the actual solution!!!
		//				print_point_to_screen_matlab(ED_d->current_linear,"curr");
		
		
		//    if (EG[oid].prec < 64)
		//    { // print footer in double precision
		//      printPathFooter_d(&trackCount_copy[oid], &EG[oid], &T_copy[oid], OUT_copy[oid], RAWOUT_copy[oid], FAIL_copy[oid], NONSOLN_copy[oid], &BED_copy[oid]);
		//    }
		//    else
		//    { // print footer in multi precision
		//      printPathFooter_mp(&trackCount_copy[oid], &EG[oid], &T_copy[oid], OUT_copy[oid], RAWOUT_copy[oid], FAIL_copy[oid], NONSOLN_copy[oid], BED_copy[oid].BED_mp);
		//    }
		
		
		
		
		
	}// re: for (ii=0; ii<W.W.num_pts ;ii++)
	
	
	
	W_new->W.num_pts=solution_counter;
	printf("found %d solutions after the detjac_to_detjac regeneration step\n",solution_counter);
	
	
	//should i re-size the W_new->W_*.pts structures according to the number of solutions???
	
	
	//clear the data structures.
	
  for (ii = 0; ii >W.W.num_pts; ii++)
  { // clear startPts[ii]
    clear_point_data_d(&startPts[ii]);
  }
  free(startPts);
	
  // clear the structures
  clear_detjactodetjac_omp_d(max, &EG, &trackCount_copy, trackCount, &OUT_copy, OUT, &RAWOUT_copy, RAWOUT, &MIDOUT_copy, MIDOUT, &FAIL_copy, FAIL, &NONSOLN_copy, NONSOLN, &T_copy, &BED_copy);
	
	//  if (!ED_d->squareSystem.noChanges)
	//  { // complete NONSOLN
	//    rewind(NONSOLN);
	//    fprintf(NONSOLN, "%d", trackCount->junkCount);
	//    fclose(NONSOLN);
	//  }
	
	
	
	
	
  return;
}




// derived from zero_dim_track_path_d
void detjac_to_detjac_track_path_d(int pathNum, endgame_data_t *EG_out,
																	 point_data_d *Pin,
																	 FILE *OUT, FILE *MIDOUT, tracker_config_t *T,
																	 void const *ED_d, void const *ED_mp,
																	 int (*eval_func_d)(point_d, point_d, vec_d, mat_d, mat_d, point_d, comp_d, void const *),
																	 int (*eval_func_mp)(point_mp, point_mp, vec_mp, mat_mp, mat_mp, point_mp, comp_mp, void const *),
																	 int (*change_prec)(void const *, int),
																	 int (*find_dehom)(point_d, point_mp, int *, point_d, point_mp, int, void const *, void const *))
/***************************************************************\
 * USAGE:                                                        *
 * ARGUMENTS:                                                    *
 * RETURN VALUES:                                                *
 * NOTES: actually does the zero-dimensional tracking and sets   *
 *  up EG_out                                                    *
 \***************************************************************/
{
	
	
	
	EG_out->pathNum = pathNum;
  EG_out->codim = 0; // zero dimensional - this is ignored
	
  T->first_step_of_path = 1;
	
	//	printf("mytype %d\n",T->MPType);
  if (T->MPType == 2)
  { // track using AMP
		
		//verification code:
		//		detjactodetjac_eval_data_mp *LtLED = (detjactodetjac_eval_data_mp *)ED_mp; // to avoid having to cast every time
		//		print_matrix_to_screen_matlab_mp(LtLED->n_minusone_randomizer_matrix,"nminus1");
		//		print_point_to_screen_matlab_mp(LtLED->old_linear,"old");
		//		print_point_to_screen_matlab_mp(LtLED->current_linear,"curr");
		//		print_matrix_to_screen_matlab_mp(LtLED->patch.patchCoeff,"patch");
		//		printf("patch precision %d\n",LtLED->patch.curr_prec);
		
    EG_out->prec = EG_out->last_approx_prec = 52;
		
		EG_out->retVal = endgame_amp(T->endgameNumber, EG_out->pathNum, &EG_out->prec, &EG_out->first_increase, &EG_out->PD_d, &EG_out->PD_mp, &EG_out->last_approx_prec, EG_out->last_approx_d, EG_out->last_approx_mp, Pin, T, OUT, MIDOUT, ED_d, ED_mp, eval_func_d, eval_func_mp, change_prec, find_dehom);
		
    if (EG_out->prec == 52)
    { // copy over values in double precision
      EG_out->latest_newton_residual_d = T->latest_newton_residual_d;
      EG_out->t_val_at_latest_sample_point_d = T->t_val_at_latest_sample_point;
      EG_out->error_at_latest_sample_point_d = T->error_at_latest_sample_point;
      findFunctionResidual_conditionNumber_d(&EG_out->function_residual_d, &EG_out->condition_number, &EG_out->PD_d, ED_d, eval_func_d);
    }
    else
    { // make sure that the other MP things are set to the correct precision
      mpf_clear(EG_out->function_residual_mp);
      mpf_init2(EG_out->function_residual_mp, EG_out->prec);
			
      mpf_clear(EG_out->latest_newton_residual_mp);
      mpf_init2(EG_out->latest_newton_residual_mp, EG_out->prec);
			
      mpf_clear(EG_out->t_val_at_latest_sample_point_mp);
      mpf_init2(EG_out->t_val_at_latest_sample_point_mp, EG_out->prec);
			
      mpf_clear(EG_out->error_at_latest_sample_point_mp);
      mpf_init2(EG_out->error_at_latest_sample_point_mp, EG_out->prec);
			
      // copy over the values
      mpf_set(EG_out->latest_newton_residual_mp, T->latest_newton_residual_mp);
      mpf_set_d(EG_out->t_val_at_latest_sample_point_mp, T->t_val_at_latest_sample_point);
      mpf_set_d(EG_out->error_at_latest_sample_point_mp, T->error_at_latest_sample_point);
      findFunctionResidual_conditionNumber_mp(EG_out->function_residual_mp, &EG_out->condition_number, &EG_out->PD_mp, ED_mp, eval_func_mp);
    }
  }
  else
  { // track using double precision
    EG_out->prec = EG_out->last_approx_prec = 52;
		
    EG_out->retVal = endgame_d(T->endgameNumber, EG_out->pathNum, &EG_out->PD_d, EG_out->last_approx_d, Pin, T, OUT, MIDOUT, ED_d, eval_func_d, find_dehom);  // WHERE THE ACTUAL TRACKING HAPPENS
    EG_out->first_increase = 0;
    // copy over values in double precision
    EG_out->latest_newton_residual_d = T->latest_newton_residual_d;
    EG_out->t_val_at_latest_sample_point_d = T->t_val_at_latest_sample_point;
    EG_out->error_at_latest_sample_point_d = T->error_at_latest_sample_point;
    findFunctionResidual_conditionNumber_d(&EG_out->function_residual_d, &EG_out->condition_number, &EG_out->PD_d, ED_d, eval_func_d);
  }
	
  return;
}




// derived from zero_dim_basic_setup_d
int detjac_to_detjac_setup_d(FILE **OUT, char *outName,
														 FILE **midOUT, char *midName,
														 tracker_config_t *T,
														 detjactodetjac_eval_data_d *ED,
														 prog_t *dummyProg,
														 int **startSub, int **endSub, int **startFunc, int **endFunc, int **startJvsub, int **endJvsub, int **startJv, int **endJv, int ***subFuncsBelow,
														 int (**eval_d)(point_d, point_d, vec_d, mat_d, mat_d, point_d, comp_d, void const *),
														 int (**eval_mp)(point_mp, point_mp, vec_mp, mat_mp, mat_mp, point_mp, comp_mp, void const *),
														 char *preprocFile, char *degreeFile,
														 int findStartPts, char *pointsIN, char *pointsOUT,
														 mat_mp n_minusone_randomizer_matrix_full_prec,
														 witness_set_d W,
														 vec_mp old_projection_full_prec,
														 vec_mp new_projection_full_prec)
/***************************************************************\
 * USAGE:                                                        *
 * ARGUMENTS:                                                    *
 * RETURN VALUES: number of original variables                   *
 * NOTES: setup for zero dimensional tracking                    *
 \***************************************************************/
{ // need to create the homotopy
//	printf("entering detjac_to_detjac_setup_d 613\n");
  int rank, patchType, ssType, numOrigVars, adjustDegrees, numGps;
	
  *eval_d = &detjac_to_detjac_eval_d;   //DAB
  *eval_mp = &detjac_to_detjac_eval_mp; // DAB  // lol send them to the same place for now.
	
	//  *eval_mp = &detjactodetjac_eval_mp; // DAB
	
  *OUT = fopen(outName, "w");  // open the main output files.
  *midOUT = fopen(midName, "w");
	
  if (T->MPType == 2) // using AMP - need to allocate space to store BED_mp
    ED->BED_mp = (detjactodetjac_eval_data_mp *)bmalloc(1 * sizeof(detjactodetjac_eval_data_mp));
  else
    ED->BED_mp = NULL;
	
	
  // setup a straight-line program, using the file(s) created by the parser
  T->numVars = numOrigVars = setupProg_count(dummyProg, T->Precision, T->MPType, startSub, endSub, startFunc, endFunc, startJvsub, endJvsub, startJv, endJv, subFuncsBelow);
	
  // setup preProcData
  setupPreProcData(preprocFile, &ED->preProcData);
	
	
	
  numGps = ED->preProcData.num_var_gp + ED->preProcData.num_hom_var_gp;
  // find the rank
  rank = rank_finder_d(&ED->preProcData, dummyProg, T, T->numVars);
  // check to make sure that it is possible to have a zero dimensional component
	//  if (T->numVars > rank + numGps)
	//		//  {
	//		//    printf("The system has no zero dimensional solutions based on its rank!\n");
	//		//    printf("The rank of the system including the patches is %d while the total number of variables is %d.\n\n", rank + numGps, T->numVars);
	//		//    bexit(ERROR_INPUT_SYSTEM);
	//		//  }
	//
	//		//AM I ACUTALLY SUPPOSED TO DO THIS? !!! HERE
	//		// adjust the number of variables based on the rank
	//	T->numVars = rank + numGps;
	
	
	
	
	//  // now that we know the rank, we can setup the rest of ED
	//  if (numGps == 1)
	//  { // 1-hom
	patchType = 2; // 1-hom patch
	ssType = 0;    // with 1-hom, we use total degree start system
	adjustDegrees = 0; // if the system does not need its degrees adjusted, then that is okay
	setupdetjactodetjacEval_d(T,preprocFile, degreeFile, dummyProg, rank, patchType, ssType, T->MPType, &T->numVars, NULL, NULL, NULL, ED, adjustDegrees, n_minusone_randomizer_matrix_full_prec, W,old_projection_full_prec,new_projection_full_prec);
	//  }
	//  else
	//  { // m-hom, m > 1
	//    patchType = 0; // random patch based on m-hom variable structure
	//    ssType = 1;    // with m-hom, we use the mhom structure for start system
	//    adjustDegrees = 0; // if the system does not need its degrees adjusted, then that is okay
	//    setupdetjactodetjacEval_d(T,preprocFile, degreeFile, dummyProg, rank, patchType, ssType, T->MPType, &ED->preProcData, NULL, NULL, NULL, ED, adjustDegrees, n_minusone_randomizer_matrix_full_prec, W);
	//  }
	
	
  return numOrigVars;
}




//this derived from basic_eval_d
int detjac_to_detjac_eval_d(point_d funcVals, point_d parVals, vec_d parDer, mat_d Jv, mat_d Jp, point_d current_variable_values, comp_d pathVars, void const *ED)
{ // evaluates a special homotopy type, built for bertini_real
//	printf("t = %lf+1i*%lf;\n", pathVars->r, pathVars->i);
	
  detjactodetjac_eval_data_d *BED = (detjactodetjac_eval_data_d *)ED; // to avoid having to cast every time
	
  int ii, jj, kk;
  comp_d one_minus_s, gamma_s;
	
	set_one_d(one_minus_s);
  sub_d(one_minus_s, one_minus_s, pathVars);  // one_minus_s = (1 - s)
  mul_d(gamma_s, BED->gamma, pathVars);       // gamma_s = gamma * s
	
  // we assume that the only parameter is s = t and setup parVals & parDer accordingly.
	
	
	
	vec_d patchValues; init_vec_d(patchValues, 0);
	vec_d temp_function_values; init_vec_d(temp_function_values,0);
	vec_d AtimesF;  init_vec_d(AtimesF,0);
	
	
	mat_d Jv_Patch; init_mat_d(Jv_Patch, 0, 0);
	mat_d tempmat; init_mat_d(tempmat,0,0);
	mat_d AtimesJ; init_mat_d(AtimesJ,1,1);
	mat_d Jv_detjac_old; init_mat_d(Jv_detjac_old,0,0);
	mat_d Jv_detjac_new; init_mat_d(Jv_detjac_new,0,0);
	mat_d temp_jacobian_functions, temp_jacobian_parameters; init_mat_d(temp_jacobian_functions,0,0); init_mat_d(temp_jacobian_parameters,0,0);
	
	
	

	comp_d detjac_old;
	comp_d detjac_new;
	comp_d temp, temp2;
	
	
	
	evalProg_d(temp_function_values, parVals, parDer, temp_jacobian_functions, temp_jacobian_parameters, current_variable_values, pathVars, BED->SLP);
	
	
  // evaluate the patch
  patch_eval_d(    patchValues, parVals, parDer, Jv_Patch, Jp, current_variable_values, pathVars, &BED->patch);  // Jp is ignored
	

	
  // combine everything
	
	change_size_vec_d(funcVals,BED->num_variables);
  change_size_mat_d(Jv, BED->num_variables, BED->num_variables);
  change_size_mat_d(Jp, BED->num_variables, 1);
	
	
  funcVals->size = Jv->rows = Jp->rows = BED->num_variables;
  Jv->cols = BED->num_variables;  //  -> this should be square!!!
  Jp->cols = 1;
	
	mat_mul_d(AtimesJ,BED->n_minusone_randomizer_matrix,temp_jacobian_functions);
	
	
	mul_mat_vec_d(AtimesF,BED->n_minusone_randomizer_matrix, temp_function_values ); // set values of AtimesF (A is randomization matrix)
	
	for (ii=0; ii<AtimesF->size; ii++) { // for each function, after (real orthogonal) randomization
		set_d(&funcVals->coord[ii], &AtimesF->coord[ii]);
	}
	
	
	
	
	
	

	
	
	//the determinant of the jacobian, with the projection.
	
	mat_cp_d(tempmat,AtimesJ);// copy into the matrix of which we will take the determinant
	
	increase_size_mat_d(tempmat,BED->num_variables,BED->num_variables); // make it bigger to accomodate more entries.  make square
	tempmat->rows = tempmat->cols = BED->num_variables; // change the size indicators
	
	//copy in the jacocian of the patch equation
	for (ii=0; ii<BED->num_variables; ++ii) {
		set_d(&tempmat->entry[BED->num_variables-1][ii],&Jv_Patch->entry[0][ii]); //  copy in the patch jacobian
	}
	

	//copy in the OLD projection from BED
	for (ii=0; ii<BED->num_variables; ++ii) {
		set_d(&tempmat->entry[BED->num_variables-2][ii],&BED->old_projection->coord[ii]); // copy in the old_projection
	}
	//now TAKE THE DETERMINANT of tempmat.
//	print_matrix_to_screen_matlab(tempmat,"det1");
	take_determinant_d(detjac_old,tempmat); // the determinant goes into detjac
	mul_d(temp,detjac_old,gamma_s);
	
	
	
	//copy in the NEW projection from BED
	for (ii=0; ii<BED->num_variables; ++ii) {
		set_d(&tempmat->entry[BED->num_variables-2][ii],&BED->new_projection->coord[ii]); // copy in the old_projection
	}
	//now TAKE THE DETERMINANT of tempmat.
//	print_matrix_to_screen_matlab(tempmat,"det2");
	take_determinant_d(detjac_new,tempmat); // the determinant goes into detjac
	mul_d(temp2,detjac_new,one_minus_s);
	
	add_d(&funcVals->coord[BED->num_variables-2],temp,temp2);  // (1-s)detjac_new + gamma*s detjac_old

	
	
	
	//set the PATCH values
	int offset = BED->num_variables-1;
	for (ii=0; ii<BED->patch.num_patches; ii++) {
		set_d(&funcVals->coord[ii+offset], &patchValues->coord[ii]);
	}
	
	
	
	
	
	//now for the NUMERICAL DERIVATIVE.
	
	detjac_numerical_derivative_d(Jv_detjac_old, // return value
																current_variable_values, pathVars, BED->old_projection,
																BED->num_variables,
																BED->patch,
																BED->SLP,
																BED->n_minusone_randomizer_matrix); // input parameters for the method
	
	detjac_numerical_derivative_d(Jv_detjac_new, // return value
																current_variable_values, pathVars, BED->new_projection,
																BED->num_variables,
																BED->patch,
																BED->SLP,
																BED->n_minusone_randomizer_matrix); // input parameters for the method

//	print_point_to_screen_matlab(current_variable_values,"current_variable_values");
	
//	print_matrix_to_screen_matlab(Jv_detjac_new,"Jv_new");
//	printf("%lf %lf\n",Jv_detjac_old->entry[0][0].r, Jv_detjac_old->entry[0][0].i);
//	printf("%lf %lf\n",Jv_detjac_new->entry[0][0].r, Jv_detjac_new->entry[0][0].i);
//	mypause();
	//////////////
	//
	// SET THE FINAL RETURNED JACOBIAN ENTRIES.
	//
	////////////////////
	// note: matrix was sized appropriately above.
	
	
	//first, the entries related to the functions
  for (ii = 0; ii < BED->num_variables-2; ii++)
  {
		for (jj = 0; jj < BED->num_variables; jj++)
		{
			set_d(&Jv->entry[ii][jj],&AtimesJ->entry[ii][jj]);
		}
  }
	
	
	
	
	
	// the entries in the jacobian for the detjac terms.
	
	

	offset = BED->num_variables-2;
	for (ii=0; ii<BED->num_variables; ii++) {
		mul_d(temp, &Jv_detjac_old->entry[0][ii],gamma_s); // temp = d/dx detjac_old * gamma * s
		mul_d(temp2,&Jv_detjac_new->entry[0][ii],one_minus_s);// temp2 = d/dx detjac_new * (1-s)
		add_d(&Jv->entry[offset][ii],temp, temp2);  // Jv = (1-s)d/dx(detjac) + s*gamma*d/dx detjac
	}
	//&&&&&&&&&&&
	
	
	// the last entries come from the patch equation(s)
	offset = BED->num_variables-1;
	for (ii=0; ii<BED->num_variables; ii++) {
		set_d(&Jv->entry[offset][ii],&Jv_Patch->entry[0][ii]);
	}
	
	
	for (ii = 0; ii<BED->num_variables-2; ii++) {
		set_zero_d(&Jp->entry[ii][0]);  // no parameter dependence means zero derivative for these functions
	}
	
	
	//initialize
	//Jp[N-2][0] = -detjac_new + gamma*detjac_old
	
	
	set_d(&Jp->entry[BED->num_variables-2][0],detjac_new);
	neg_d(&Jp->entry[BED->num_variables-2][0],&Jp->entry[BED->num_variables-2][0]);
	
	mul_d(temp,BED->gamma,detjac_old);  // gamma * detjac_old
	add_d(&Jp->entry[BED->num_variables-2][0],&Jp->entry[BED->num_variables-2][0],temp);
	
	
	
	
	// the entries in the jacobian for the patch equations.
	offset = BED->num_variables-1;
	for (ii = 0; ii<BED->patch.num_patches; ii++)  // for each patch equation
	{ // funcVals = patchValues
		// Jp = 0
		set_zero_d(&Jp->entry[ii+offset][0]);
		// Jv = Jv_Patch
		for (jj = 0; jj<BED->num_variables; jj++) // for each variable
		{
			set_d(&Jv->entry[ii+offset][jj], &Jv_Patch->entry[ii][jj]);
		}
		
	}
	
	
	
	
	// set parVals & parDer correctly
  change_size_point_d(parVals, 1);
  change_size_vec_d(parDer, 1);
	
  parVals->size = parDer->size = 1;
  set_d(&parVals->coord[0], pathVars); // s = t
  set_one_d(&parDer->coord[0]);       // ds/dt = 1
	
	
	
//	print_matrix_to_screen_matlab( AtimesJ,"jac");
//	print_point_to_screen_matlab(current_variable_values,"currvars");

//	print_point_to_screen_matlab(BED->new_projection,"projnew");
//	print_matrix_to_screen_matlab(Jv_detjac,"Jv_detjac");
//	print_point_to_screen_matlab(linprod_derivative,"linprod_derivative");
//	print_point_to_screen_matlab(funcVals,"F");
//	print_point_to_screen_matlab(parVals,"parVals");
//	print_point_to_screen_matlab(parDer,"parDer");
//	print_matrix_to_screen_matlab(Jv,"Jv");
//	print_matrix_to_screen_matlab(Jp,"Jp");

	
	//these values are set in this function:  point_d funcVals, point_d parVals, vec_d parDer, mat_d Jv, mat_d Jp
//	print_matrix_to_screen_matlab(BED->n_minusone_randomizer_matrix,"n_minusone_randomizer_matrix");
//
//	mypause();
//

	
	
	
	clear_mat_d(temp_jacobian_functions);
	clear_mat_d(temp_jacobian_parameters);
	clear_mat_d(AtimesJ);
	clear_mat_d(tempmat);
	clear_mat_d(Jv_detjac_new);
	clear_mat_d(Jv_detjac_old);
	clear_mat_d(Jv_Patch);
	
	clear_vec_d(patchValues);
	clear_vec_d(temp_function_values);
	clear_vec_d(AtimesF);

	//	printf("exiting eval\n");
  return 0;
}


void printdetjactodetjacRelevantData(detjactodetjac_eval_data_d *ED_d, detjactodetjac_eval_data_mp *ED_mp, int MPType, int eqbyeqMethod, FILE *FP)
/***************************************************************\
 * USAGE:                                                        *
 * ARGUMENTS:                                                    *
 * RETURN VALUES:                                                *
 * NOTES: prints the relevant data to FP so that we can recover  *
 * the system if needed for sharpening                           *
 \***************************************************************/
{
  // print the MPType and if an eq-by-eq method (diagona/regen) was used
	//  fprintf(FP, "%d %d\n", MPType, eqbyeqMethod);
	//
	//  // print the patch
	//  printPatchCoeff(FP, MPType, ED_d, ED_mp);
	//
	//  // print the start system
	//  printStartSystem(FP, MPType, ED_d, ED_mp);
	//
	//  // print the square system
	//  printSquareSystem(FP, MPType, ED_d, ED_mp);
	
  return;
}



void detjactodetjac_eval_clear_d(detjactodetjac_eval_data_d *ED, int clearRegen, int MPType)
/***************************************************************\
 * USAGE:                                                        *
 * ARGUMENTS:                                                    *
 * RETURN VALUES:                                                *
 * NOTES: clear ED                                               *
 \***************************************************************/
{
	//clear the patch
  patch_eval_data_clear_d(&ED->patch);
  preproc_data_clear(&ED->preProcData);
	
	
	
  if (MPType == 2)
  { // clear the MP stuff
    detjactodetjac_eval_clear_mp(ED->BED_mp, 0, 0);
  }
	
	//specifics for the detjactodetjac method.
	clear_vec_d(ED->old_projection);
	clear_vec_d(ED->new_projection);
	clear_mat_d(ED->n_minusone_randomizer_matrix);
	
	
	
  return;
}






void setup_detjac_to_detjac_omp_d(int max_threads, endgame_data_t **EG, trackingStats **trackCount_copy, trackingStats *trackCount,
																	FILE ***OUT_copy, FILE *OUT, FILE ***RAWOUT_copy, FILE *RAWOUT,
																	FILE ***MIDOUT_copy, FILE *MIDOUT, FILE ***FAIL_copy, FILE *FAIL,
																	FILE ***NONSOLN_copy, FILE *NONSOLN,
																	tracker_config_t **T_copy, tracker_config_t *T,
																	detjactodetjac_eval_data_d **BED_copy, detjactodetjac_eval_data_d *ED_d, detjactodetjac_eval_data_mp *ED_mp)
/***************************************************************\
 * USAGE:                                                        *
 * ARGUMENTS:                                                    *
 * RETURN VALUES:                                                *
 * NOTES: setup everything needed to do zero dimensional tracking*
 *  using OpenMP                                                 *
 \***************************************************************/
// if max_threads == 1, things are only pointers to the actual values,
// otherwise, they are copies
{
  int ii;
  // error checking
  if (max_threads <= 0)
  {
    printf("\n\nERROR: The number of threads (%d) needs to be positive when setting up for tracking!\n", max_threads);
    bexit(ERROR_CONFIGURATION);
  }
	
	
	
	
  // allocate space for EG
  *EG = (endgame_data_t *)bmalloc(max_threads * sizeof(endgame_data_t));
	
	// initialize
  for (ii = 0; ii < max_threads; ii++)
	{
    if (T->MPType == 2)
    { // initialize for AMP tracking
      init_endgame_data(&(*EG)[ii], 64);
    }
    else
    { // initialize for double precision tracking
      init_endgame_data(&(*EG)[ii], 52);
    }
	}
	
  // allocate space to hold pointers to the files
  *OUT_copy = (FILE **)bmalloc(max_threads * sizeof(FILE *));
  *MIDOUT_copy = (FILE **)bmalloc(max_threads * sizeof(FILE *));
  *RAWOUT_copy = (FILE **)bmalloc(max_threads * sizeof(FILE *));
  *FAIL_copy = (FILE **)bmalloc(max_threads * sizeof(FILE *));
  *NONSOLN_copy = (FILE **)bmalloc(max_threads * sizeof(FILE *));
	
  if (max_threads == 1)
  { // setup the pointers
    *trackCount_copy = trackCount;
    *T_copy = T;
    *BED_copy = ED_d;
    (*BED_copy)->BED_mp = ED_mp; // make sure that this is pointed to inside of ED_d
		
    (*OUT_copy)[0] = OUT;
    (*RAWOUT_copy)[0] = RAWOUT;
    (*MIDOUT_copy)[0] = MIDOUT;
    (*FAIL_copy)[0] = FAIL;
    (*NONSOLN_copy)[0] = NONSOLN;
  }
  else // max_threads > 1
  { // allocate memory
    *trackCount_copy = (trackingStats *)bmalloc(max_threads * sizeof(trackingStats));
    *T_copy = (tracker_config_t *)bmalloc(max_threads * sizeof(tracker_config_t));
    *BED_copy = (detjactodetjac_eval_data_d *)bmalloc(max_threads * sizeof(detjactodetjac_eval_data_d));
		
    // copy T, ED_d, ED_mp, & trackCount
    for (ii = 0; ii < max_threads; ii++)
    { // copy T
      cp_tracker_config_t(&(*T_copy)[ii], T);
      // copy ED_d & ED_mp
      cp_detjactodetjac_eval_data_d(&(*BED_copy)[ii], ED_d, ED_mp, T->MPType);
      // initialize trackCount_copy
      init_trackingStats(&(*trackCount_copy)[ii]);
      (*trackCount_copy)[ii].numPoints = trackCount->numPoints;
    }
		
    // setup the files
    char *str = NULL;
    int size;
    for (ii = 0; ii < max_threads; ii++)
    {
      size = 1 + snprintf(NULL, 0, "output_%d", ii);
      str = (char *)brealloc(str, size * sizeof(char));
      sprintf(str, "output_%d", ii);
      (*OUT_copy)[ii] = fopen(str, "w+");
			
      size = 1 + snprintf(NULL, 0, "midout_%d", ii);
      str = (char *)brealloc(str, size * sizeof(char));
      sprintf(str, "midout_%d", ii);
      (*MIDOUT_copy)[ii] = fopen(str, "w+");
			
      size = 1 + snprintf(NULL, 0, "rawout_%d", ii);
      str = (char *)brealloc(str, size * sizeof(char));
      sprintf(str, "rawout_%d", ii);
      (*RAWOUT_copy)[ii] = fopen(str, "w+");
			
      size = 1 + snprintf(NULL, 0, "fail_%d", ii);
      str = (char *)brealloc(str, size * sizeof(char));
      sprintf(str, "fail_%d", ii);
      (*FAIL_copy)[ii] = fopen(str, "w+");
			
      size = 1 + snprintf(NULL, 0, "nonsolutions_%d", ii);
      str = (char *)brealloc(str, size * sizeof(char));
      sprintf(str, "nonsolutions_%d", ii);
      (*NONSOLN_copy)[ii] = fopen(str, "w+");
    }
    free(str);
  }
	
	
  return;
}

void clear_detjactodetjac_omp_d(int max_threads, endgame_data_t **EG, trackingStats **trackCount_copy, trackingStats *trackCount, FILE ***OUT_copy, FILE *OUT, FILE ***RAWOUT_copy, FILE *RAWOUT, FILE ***MIDOUT_copy, FILE *MIDOUT, FILE ***FAIL_copy, FILE *FAIL, FILE ***NONSOLN_copy, FILE *NONSOLN, tracker_config_t **T_copy, detjactodetjac_eval_data_d **BED_copy)
/***************************************************************\
 * USAGE:                                                        *
 * ARGUMENTS:                                                    *
 * RETURN VALUES:                                                *
 * NOTES: copy the relevant data back to the standard spot and   *
 *  clear the allocated data that was used by OpenMP             *
 \***************************************************************/
// if max_threads == 1, things are only pointers to the actual values,
// otherwise, they are copies
{
  int ii;
	
  // clear EG
  for (ii = max_threads - 1; ii >= 0; ii--)
  {
    clear_endgame_data(&(*EG)[ii]);
  }
  free(*EG);
	
  if (max_threads == 1)
  { // set the pointers to NULL since they just pointed to the actual values
    *trackCount_copy = NULL;
    *T_copy = NULL;
    *BED_copy = NULL;
		
    *OUT_copy[0] = NULL;
    *RAWOUT_copy[0] = NULL;
    *MIDOUT_copy[0] = NULL;
    *FAIL_copy[0] = NULL;
		
    // free the memory of the file pointers
    free(*OUT_copy);
    free(*MIDOUT_copy);
    free(*RAWOUT_copy);
    free(*FAIL_copy);
    free(*NONSOLN_copy);
  }
  else if (max_threads > 1)
  {
    // combine trackCount_copy
    add_trackingStats(trackCount, *trackCount_copy, max_threads);
		
    // clear the copies T, ED_d & ED_mp
    for (ii = max_threads - 1; ii >= 0; ii--)
    { // clear BED_copy - 0 since not using regeneration
      detjactodetjac_eval_clear_d(&(*BED_copy)[ii], 0, (*T_copy)[ii].MPType);
			// clear T_copy
      tracker_config_clear(&(*T_copy)[ii]);
    }
		
    // free the memory
    free(*trackCount_copy);
    free(*T_copy);
    free(*BED_copy);
		
    // copy all of the files to the appropriate place
    char ch, *str = NULL;
    int size;
    for (ii = 0; ii < max_threads; ii++)
    {
      // rewind to beginning for reading
      rewind((*OUT_copy)[ii]);
      // copy over to OUT
      ch = fgetc((*OUT_copy)[ii]);
      while (ch != EOF)
      {
        fprintf(OUT, "%c", ch);
        ch = fgetc((*OUT_copy)[ii]);
      }
      // close file & delete
      fclose((*OUT_copy)[ii]);
      size = 1 + snprintf(NULL, 0, "output_%d", ii);
      str = (char *)brealloc(str, size * sizeof(char));
      sprintf(str, "output_%d", ii);
      remove(str);
			
      // rewind to beginning for reading
      rewind((*MIDOUT_copy)[ii]);
      // copy over to MIDOUT
      ch = fgetc((*MIDOUT_copy)[ii]);
      while (ch != EOF)
      {
        fprintf(MIDOUT, "%c", ch);
        ch = fgetc((*MIDOUT_copy)[ii]);
      }
      // close file & delete
      fclose((*MIDOUT_copy)[ii]);
      size = 1 + snprintf(NULL, 0, "midout_%d", ii);
      str = (char *)brealloc(str, size * sizeof(char));
      sprintf(str, "midout_%d", ii);
      remove(str);
			
      // rewind to beginning for reading
      rewind((*RAWOUT_copy)[ii]);
      // copy over to RAWOUT
      ch = fgetc((*RAWOUT_copy)[ii]);
      while (ch != EOF)
      {
        fprintf(RAWOUT, "%c", ch);
        ch = fgetc((*RAWOUT_copy)[ii]);
      }
      // close file & delete
      fclose((*RAWOUT_copy)[ii]);
      size = 1 + snprintf(NULL, 0, "rawout_%d", ii);
      str = (char *)brealloc(str, size * sizeof(char));
      sprintf(str, "rawout_%d", ii);
      remove(str);
			
      // rewind to beginning for reading
      rewind((*FAIL_copy)[ii]);
      // copy over to FAIL
      ch = fgetc((*FAIL_copy)[ii]);
      while (ch != EOF)
      {
        fprintf(FAIL, "%c", ch);
        ch = fgetc((*FAIL_copy)[ii]);
      }
      // close file & delete
      fclose((*FAIL_copy)[ii]);
      size = 1 + snprintf(NULL, 0, "fail_%d", ii);
      str = (char *)brealloc(str, size * sizeof(char));
      sprintf(str, "fail_%d", ii);
      remove(str);
			
      // rewind to beginning for reading
      rewind((*NONSOLN_copy)[ii]);
      // copy over to NONSOLN
      ch = fgetc((*NONSOLN_copy)[ii]);
      while (ch != EOF)
      {
        fprintf(NONSOLN, "%c", ch);
        ch = fgetc((*NONSOLN_copy)[ii]);
      }
      // close file & delete
      fclose((*NONSOLN_copy)[ii]);
      size = 1 + snprintf(NULL, 0, "nonsolutions_%d", ii);
      str = (char *)brealloc(str, size * sizeof(char));
      sprintf(str, "nonsolutions_%d", ii);
      remove(str);
    }
    free(str);
    // free file memory
    free(*OUT_copy);
    free(*MIDOUT_copy);
    free(*RAWOUT_copy);
    free(*FAIL_copy);
    free(*NONSOLN_copy);
  }
	
  return;
}




void setupdetjactodetjacEval_d(tracker_config_t *T,char preprocFile[], char degreeFile[], prog_t *dummyProg,
															 int squareSize, int patchType, int ssType, int MPType,
															 void const *ptr1, void const *ptr2, void const *ptr3, void const *ptr4,// what are these supposed to point to?
															 detjactodetjac_eval_data_d *BED, int adjustDegrees,
															 mat_mp n_minusone_randomizer_matrix_full_prec,
															 witness_set_d W,
															 vec_mp old_projection_full_prec,
															 vec_mp new_projection_full_prec)
{
  int ii;
	BED->num_variables = W.num_variables;
	printf("setup detjactodetjac_d 1295\n");
	
  setupPreProcData(preprocFile, &BED->preProcData);
	//  setupSquareSystem_d(dummyProg, squareSize, &BED->preProcData, degreeFile, &BED->squareSystem, adjustDegrees); // NOTE: squareSystem must be setup before the patch!!!
  setupPatch_d(patchType, &BED->patch, ptr1, ptr2);
	for (ii = 0; ii < BED->num_variables ; ii++)
	{
		set_d(&BED->patch.patchCoeff->entry[0][ii],&W.patch[0]->coord[ii]);
	}
	
	
	
	
	
	BED->SLP = dummyProg;
	
	
	init_mat_d(BED->n_minusone_randomizer_matrix,
							n_minusone_randomizer_matrix_full_prec->rows,n_minusone_randomizer_matrix_full_prec->cols);
	BED->n_minusone_randomizer_matrix->rows = n_minusone_randomizer_matrix_full_prec->rows;
	BED->n_minusone_randomizer_matrix->cols = n_minusone_randomizer_matrix_full_prec->cols;
	
	mat_mp_to_d(BED->n_minusone_randomizer_matrix,
							n_minusone_randomizer_matrix_full_prec);
	
	
	// set up the vectors to hold the two linears.
	
	
	init_vec_d(BED->old_projection,W.num_variables); BED->old_projection->size =  W.num_variables;
	vec_mp_to_d(BED->old_projection, old_projection_full_prec);
	
	init_vec_d(BED->new_projection,W.num_variables); BED->new_projection->size =  W.num_variables;
	vec_mp_to_d(BED->new_projection, new_projection_full_prec);
	
	
	
	
	get_comp_rand_d(BED->gamma); // set gamma to be random complex value
	
	
	if (MPType == 2)
  { // using AMP - initialize using 16 digits & 64-bit precison
    int digits = 16, prec = 64;
		initMP(prec);
		//		init_mp2(BED->BED_mp->gamma,prec);
		//		init_mp2(BED->BED_mp->gamma,max_precision);
		//		get_comp_rand_mp(BED->BED_mp->gamma);
		//		mp_to_d(BED->gamma,BED->BED_mp->gamma);
		BED->BED_mp->curr_prec = prec;
		
		BED->BED_mp->gamma_rat = (mpq_t *)bmalloc(2 * sizeof(mpq_t));
		//		init_rat(BED->BED_mp->gamma_rat);
		
		get_comp_rand_rat(BED->gamma, BED->BED_mp->gamma, BED->BED_mp->gamma_rat, prec, T->AMP_max_prec, 1, 1);
		BED->BED_mp->num_variables = W.num_variables;
		
		
		BED->BED_mp->SLP = BED->SLP; // assign the SLP pointer
		
		init_vec_mp2(BED->BED_mp->old_projection,W.num_variables,prec);
			BED->BED_mp->old_projection->size = W.num_variables;
		init_vec_mp2(BED->BED_mp->old_projection_full_prec,W.num_variables,T->AMP_max_prec);
			BED->BED_mp->old_projection_full_prec->size = W.num_variables;
		vec_cp_mp(BED->BED_mp->old_projection,old_projection_full_prec); //copy in the old linear
		vec_cp_mp(BED->BED_mp->old_projection_full_prec,old_projection_full_prec); //copy in the old linear
		
		init_vec_mp2(BED->BED_mp->new_projection,W.num_variables,prec);
			BED->BED_mp->new_projection->size = W.num_variables;
		init_vec_mp2(BED->BED_mp->new_projection_full_prec,W.num_variables,T->AMP_max_prec);
			BED->BED_mp->new_projection_full_prec->size = W.num_variables;
		vec_cp_mp(BED->BED_mp->new_projection,new_projection_full_prec); //copy in the old linear
		vec_cp_mp(BED->BED_mp->new_projection_full_prec,new_projection_full_prec); //copy in the old linear
		
		
		
		init_mat_mp2(BED->BED_mp->n_minusone_randomizer_matrix,1,1,prec); // initialize the randomizer matrix
		init_mat_mp2(BED->BED_mp->n_minusone_randomizer_matrix_full_prec,1,1,T->AMP_max_prec); // initialize the randomizer matrix
		
		mat_cp_mp(BED->BED_mp->n_minusone_randomizer_matrix_full_prec,n_minusone_randomizer_matrix_full_prec);
		mat_cp_mp(BED->BED_mp->n_minusone_randomizer_matrix,n_minusone_randomizer_matrix_full_prec);
		
		
		

		
		
		
		
    // setup preProcData
    setupPreProcData(preprocFile, &BED->BED_mp->preProcData);
		
		
		
		// setup the patch
    setupPatch_d_to_mp(&BED->patch, &BED->BED_mp->patch, digits, prec, patchType, dummyProg->numVars, ptr3, ptr4); // i question whether this is necessary.
		for (ii = 0; ii < BED->num_variables ; ii++)
		{
			mp_to_d(&BED->patch.patchCoeff->entry[0][ii],&W.patch_mp[0]->coord[ii]);
		}
		for (ii = 0; ii < BED->num_variables ; ii++)
		{
			set_mp(&BED->BED_mp->patch.patchCoeff->entry[0][ii],&W.patch_mp[0]->coord[ii]);
			mp_to_rat(BED->BED_mp->patch.patchCoeff_rat[0][ii],&W.patch_mp[0]->coord[ii]);
		}
		
		
		print_matrix_to_screen_matlab_mp(BED->BED_mp->patch.patchCoeff,"userpatch_mp");
		
  }//re: if mptype==2
	
	
	
	//	//     this commented code is for checking that the first point solves the patch equation.   it damn well should!
	//	print_matrix_to_screen_matlab(BED->patch.patchCoeff,"userpatch");
	//	vec_d patchValues;
	//	init_vec_d(patchValues,0);
	//	point_d parVals;
	//	init_vec_d(parVals,0);
	//	vec_d parDer;
	//	init_vec_d(parDer,0);
	//	mat_d Jv_Patch;
	//	init_mat_d(Jv_Patch,0,0);
	//	mat_d Jp;
	//	init_mat_d(Jp,0,0);
	//	comp_d pathVars;
	//	set_one_d(pathVars);
	//	patch_eval_d(    patchValues, parVals, parDer, Jv_Patch, Jp, W.W.pts[0], pathVars, &BED->patch);  // Jp is ignored
	//
	//	print_point_to_screen_matlab(patchValues,"patchvalues");
	//	print_point_to_screen_matlab(W.W.pts[0],"initialpoint");
	//	mypause();
	
	
  return;
}



void cp_detjactodetjac_eval_data_d(detjactodetjac_eval_data_d *BED, detjactodetjac_eval_data_d *BED_d_input, detjactodetjac_eval_data_mp *BED_mp_input, int MPType)
/***************************************************************\
 * USAGE:                                                        *
 * ARGUMENTS:                                                    *
 * RETURN VALUES:                                                *
 * NOTES: stores a copy of BED_(t)_input to BED                  *
 \***************************************************************/
{
	printf("entering cp_detjactodetjac_eval_data_d\nthis function needs much attention, as things which should be copied are not!\n");
	exit(-1);
  cp_preproc_data(&BED->preProcData, &BED_d_input->preProcData);
	//  cp_square_system_d(&BED->squareSystem, &BED_d_input->squareSystem);
  cp_patch_d(&BED->patch, &BED_d_input->patch);
	//  cp_start_system_d(&BED->startSystem, &BED_d_input->startSystem);
	BED->SLP = (prog_t *)bmalloc(1 * sizeof(prog_t));
  cp_prog_t(BED->SLP, BED_d_input->SLP);
	
	
	//HERE COPY THE MATRICES  DAB !!!
	
	set_d(BED->gamma, BED_d_input->gamma);
  if (MPType == 2)
  { // need to also setup MP versions since using AMP
    BED->BED_mp = (detjactodetjac_eval_data_mp *)bmalloc(1 * sizeof(detjactodetjac_eval_data_mp));
		
    cp_preproc_data(&BED->BED_mp->preProcData, &BED_mp_input->preProcData);
    // simply point to the SLP that was setup in BED
		//    cp_square_system_mp(&BED->BED_mp->squareSystem, &BED_mp_input->squareSystem, 0, BED->squareSystem.Prog);
    cp_patch_mp(&BED->BED_mp->patch, &BED_mp_input->patch);
		//    cp_start_system_mp(&BED->BED_mp->startSystem, &BED_mp_input->startSystem);
  }
  else
    BED->BED_mp = NULL;
	
  return;
}




int detjactodetjac_dehom(point_d out_d, point_mp out_mp, int *out_prec, point_d in_d, point_mp in_mp, int in_prec, void const *ED_d, void const *ED_mp)
/***************************************************************\
 * USAGE:                                                        *
 * ARGUMENTS:                                                    *
 * RETURN VALUES:                                                *
 * NOTES: compute the dehom point                                *
 \***************************************************************/
{
	//  basic_eval_data_d *BED_d = NULL;
	//  basic_eval_data_mp *BED_mp = NULL;
	
  *out_prec = in_prec;
	
	
	
  if (in_prec < 64)
  { // compute out_d
		//    BED_d = (basic_eval_data_d *)ED_d;
		point_cp_d(out_d,in_d);
		//    getDehomPoint_d(out_d, in_d, in_d->size, &BED_d->preProcData);
  }
  else
  { // compute out_mp
		//    BED_mp = (basic_eval_data_mp *)ED_mp;
    // set prec on out_mp
    setprec_point_mp(out_mp, *out_prec);
		point_cp_mp(out_mp,in_mp);
		//    getDehomPoint_mp(out_mp, in_mp, in_mp->size, &BED_mp->preProcData);
  }
	
	//  BED_d = NULL;
	//  BED_mp = NULL;
	
  return 0;
}









int change_detjactodetjac_eval_prec(void const *ED, int prec)
/***************************************************************\
 * USAGE:                                                        *
 * ARGUMENTS:                                                    *
 * RETURN VALUES:                                                *
 * NOTES: change precision for standard zero dimensional solving *
 \***************************************************************/
{
  change_detjactodetjac_eval_prec_mp(prec, (detjactodetjac_eval_data_mp *)ED);
  return 0;
}

void change_detjactodetjac_eval_prec_mp(int new_prec, detjactodetjac_eval_data_mp *BED)
/***************************************************************\
 * USAGE:                                                        *
 * ARGUMENTS:                                                    *
 * RETURN VALUES:                                                *
 * NOTES: change precision for the main part of BED              *
 \***************************************************************/
{
	
	int ii;
	
  // change the precision for the patch
  changePatchPrec_mp(new_prec, &BED->patch);
	
	
	if (new_prec != BED->curr_prec){
		
		printf("prec now %d\n",new_prec);
		
		BED->SLP->precision = new_prec;
		
		BED->curr_prec = new_prec;
		
		
		setprec_mp(BED->gamma, new_prec);
		mpf_set_q(BED->gamma->r, BED->gamma_rat[0]);
		mpf_set_q(BED->gamma->i, BED->gamma_rat[1]);
		
		
		change_prec_mat_mp(BED->n_minusone_randomizer_matrix,new_prec);
		mat_cp_mp(BED->n_minusone_randomizer_matrix,BED->n_minusone_randomizer_matrix_full_prec);
		
		change_prec_point_mp(BED->old_projection,new_prec);
		vec_cp_mp(BED->old_projection, BED->old_projection_full_prec);
		
		change_prec_point_mp(BED->new_projection,new_prec);
		vec_cp_mp(BED->new_projection, BED->new_projection_full_prec);
		
	}
	
	
	
	
  return;
}














int detjac_to_detjac_solver_mp(int MPType, //, double parse_time, unsigned int currentSeed
															 witness_set_d W,  // includes the initial linear.
															 mat_mp n_minusone_randomizer_matrix_full_prec,  // for randomizing down to N-1 equations.
															 vec_mp old_projection_full_prec,   // collection of random complex linears.  for setting up the regeneration for V(f\\g)
															 vec_mp new_projection_full_prec,
															 witness_set_d *W_new)
/***************************************************************\
 * USAGE:                                                        *
 * ARGUMENTS:                                                    *
 * RETURN VALUES:                                                *
 * NOTES:                                                        *
 \***************************************************************/
{
	double parse_time = 0;
  FILE *OUT = NULL, *FAIL = fopen("failed_paths", "w"), *midOUT = NULL, *rawOUT = fopen("raw_data", "w");
  tracker_config_t T;
  prog_t dummyProg;
  bclock_t time1, time2;
  int num_variables = 0, convergence_failures = 0, sharpening_failures = 0, sharpening_singular = 0, num_crossings = 0, num_sols = 0;
	
	//necessary for the setupConfig call
	double midpoint_tol, intrinsicCutoffMultiplier;
	int userHom = 0, useRegen = 0, regenStartLevel = 0, maxCodim = 0, specificCodim = 0, pathMod = 0, reducedOnly = 0, supersetOnly = 0, paramHom = 0;
	//end necessaries for the setupConfig call.
	
  int usedEq = 0;
  int *startSub = NULL, *endSub = NULL, *startFunc = NULL, *endFunc = NULL, *startJvsub = NULL, *endJvsub = NULL, *startJv = NULL, *endJv = NULL, **subFuncsBelow = NULL;
  int (*ptr_to_eval_d)(point_d, point_d, vec_d, mat_d, mat_d, point_d, comp_d, void const *) = NULL;
  int (*ptr_to_eval_mp)(point_mp, point_mp, vec_mp, mat_mp, mat_mp, point_mp, comp_mp, void const *) = NULL;
	
  detjactodetjac_eval_data_mp ED;  // was basic_eval_data_d  DAB
  trackingStats trackCount;
  char inputName[] = "func_input";
  double track_time;
	
  bclock(&time1); // initialize the clock.
  init_trackingStats(&trackCount); // initialize trackCount to all 0
	
  // setup T
  setupConfig(&T, &midpoint_tol, &userHom, &useRegen, &regenStartLevel, &maxCodim, &specificCodim, &pathMod, &intrinsicCutoffMultiplier, &reducedOnly, &supersetOnly, &paramHom, 1);
	
	// initialize latest_newton_residual_mp
  mpf_init(T.latest_newton_residual_mp);   //<------ THIS LINE IS ABSOLUTELY CRITICAL TO CALL
	
	
	//  // call the setup function
	// setup for standard tracking - 'useRegen' is used to determine whether or not to setup 'start'
	num_variables = detjac_to_detjac_setup_mp(&OUT, "output",
																						&midOUT, "midpath_data",
																						&T, &ED,
																						&dummyProg,  //arg 7
																						&startSub, &endSub, &startFunc, &endFunc,
																						&startJvsub, &endJvsub, &startJv, &endJv, &subFuncsBelow,
																						&ptr_to_eval_d, &ptr_to_eval_mp,  //args 17,18
																						"preproc_data", "deg.out",
																						!useRegen, "nonhom_start", "start",
																						n_minusone_randomizer_matrix_full_prec,W,
																						old_projection_full_prec,
																						new_projection_full_prec);
  
	int (*change_prec)(void const *, int) = NULL;
	change_prec = &change_basic_eval_prec;
	
	int (*dehom)(point_d, point_mp, int *, point_d, point_mp, int, void const *, void const *) = NULL;
	dehom = &detjactodetjac_dehom;
	
	
	
  // error checking
  if (userHom <= 0 && paramHom != 2)
  { // no pathvariables or parameters allowed!
    if (dummyProg.numPathVars > 0)
    { // path variable present
      printf("ERROR: Bertini does not expect path variables when user-defined homotopies are not being used!\n");
      bexit(ERROR_INPUT_SYSTEM);
    }
    if (dummyProg.numPars > 0)
    { // parameter present
      printf("ERROR: Bertini does not expect parameters when user-defined homotopies are not being used!\n");
      bexit(ERROR_INPUT_SYSTEM);
    }
  }
	
	
	
	if (T.endgameNumber == 3)
	{ // use the track-back endgame
		//        zero_dim_trackBack_d(&trackCount, OUT, rawOUT, midOUT, StartPts, FAIL, pathMod, &T, &ED, ED.BED_mp, ptr_to_eval_d, ptr_to_eval_mp, change_basic_eval_prec, zero_dim_dehom);
		printf("bertini_real not equipped to deal with endgameNumber 3\nexiting\n");
		exit(-99);
	}
	else
	{ // use regular endgame
		detjac_to_detjac_track_mp(&trackCount, OUT, rawOUT, midOUT,
															W,  // was the startpts file pointer.
															W_new,
															FAIL, pathMod,
															&T, &ED,
															ptr_to_eval_mp, //ptr_to_eval_d,
															change_prec, dehom);
	}
	
	
	
	
	fclose(midOUT);
	
	
	
	
	// finish the output to rawOUT
	fprintf(rawOUT, "%d\n\n", -1);  // bottom of rawOUT
	
	// check for path crossings
	midpoint_checker(trackCount.numPoints, num_variables, midpoint_tol, &num_crossings);
	
	// setup num_sols
	num_sols = trackCount.successes;
	
  // we report how we did with all paths:
  bclock(&time2);
  totalTime(&track_time, time1, time2);
  if (T.screenOut)
  {
    printf("Number of failures:  %d\n", trackCount.failures);
    printf("Number of successes:  %d\n", trackCount.successes);
    printf("Number of paths:  %d\n", trackCount.numPoints);
    printf("Parse Time = %fs\n", parse_time);
    printf("Track Time = %fs\n", track_time);
  }
  fprintf(OUT, "Number of failures:  %d\n", trackCount.failures);
  fprintf(OUT, "Number of successes:  %d\n", trackCount.successes);
  fprintf(OUT, "Number of paths:  %d\n", trackCount.numPoints);
  fprintf(OUT, "Parse Time = %fs\n", parse_time);
  fprintf(OUT, "Track Time = %fs\n", track_time);
	
  // print the system to rawOUT
	//  printdetjactodetjacRelevantData(NULL, &ED, T.MPType, usedEq, rawOUT);
	//	printZeroDimRelevantData(&ED, ED.BED_mp, T.MPType, usedEq, rawOUT);// legacy
	
  // close all of the files
  fclose(OUT);
  fclose(rawOUT);
  fprintf(FAIL, "\n");
  fclose(FAIL);
	
  // reproduce the input file needed for this run
	//  reproduceInputFile(inputName, "func_input", &T, 0, 0, currentSeed, pathMod, userHom, useRegen, regenStartLevel, maxCodim, specificCodim, intrinsicCutoffMultiplier, reducedOnly, supersetOnly, paramHom);
	
	
	
  // do the standard post-processing
	//  sort_points(num_crossings, &convergence_failures, &sharpening_failures, &sharpening_singular, inputName, num_sols, num_variables, midpoint_tol, T.final_tol_times_mult, &T, &ED.preProcData, useRegen == 1 && userHom == 0, userHom == -59);
	
	
	
  // print the failure summary
	//  printFailureSummary(&trackCount, convergence_failures, sharpening_failures, sharpening_singular);
	
	
	free(startSub);
	free(endSub);
	free(startFunc);
	free(endFunc);
	free(startJvsub);
	free(endJvsub);
	free(startJv);
	free(endJv);
	
	
	
  detjactodetjac_eval_clear_mp(&ED, userHom, T.MPType);
  tracker_config_clear(&T);
	
  return 0;
}




void detjac_to_detjac_track_mp(trackingStats *trackCount,
															 FILE *OUT, FILE *RAWOUT, FILE *MIDOUT,
															 witness_set_d W,
															 witness_set_d *W_new,
															 FILE *FAIL,
															 int pathMod, tracker_config_t *T,
															 detjactodetjac_eval_data_mp *ED,
															 int (*eval_func_mp)(point_mp, point_mp, vec_mp, mat_mp, mat_mp, point_mp, comp_mp, void const *),
															 int (*change_prec)(void const *, int),
															 int (*find_dehom)(point_d, point_mp, int *, point_d, point_mp, int, void const *, void const *))
/***************************************************************\
 * USAGE:                                                        *
 * ARGUMENTS:                                                    *
 * RETURN VALUES:                                                *
 * NOTES: does standard zero dimensional tracking                *
 *  in either double precision or adaptive precision             *
 \***************************************************************/
{
	
  int ii,jj,kk, oid, startPointIndex, max = max_threads();
  tracker_config_t *T_copy = NULL;
  detjactodetjac_eval_data_mp *BED_copy = NULL;
  trackingStats *trackCount_copy = NULL;
  FILE **OUT_copy = NULL, **MIDOUT_copy = NULL, **RAWOUT_copy = NULL, **FAIL_copy = NULL, *NONSOLN = NULL, **NONSOLN_copy = NULL;
	
  // top of RAWOUT - number of variables and that we are doing zero dimensional
  fprintf(RAWOUT, "%d\n%d\n", T->numVars, 0);
	
	
	
	// setup NONSOLN
	//  if (!ED_d->squareSystem.noChanges)
	//  { // setup NONSOLN
	//    NONSOLN = fopen("nonsolutions", "w");
	//    fprintf(NONSOLN, "                                    \n\n");
	//  }
	
	
  int (*curr_eval_mp)(point_mp, point_mp, vec_mp, mat_mp, mat_mp, point_mp, comp_mp, void const *) = NULL;
	
  curr_eval_mp = &detjac_to_detjac_eval_mp; //
	
	
	point_data_mp *startPts = NULL;
	startPts = (point_data_mp *)bmalloc(W.W_mp.num_pts * sizeof(point_data_mp));
	
	
	
	for (ii = 0; ii < W.W_mp.num_pts; ii++)
	{ // setup startPts[ii]
		init_point_data_mp2(&startPts[ii], W.num_variables, T->Precision);
		startPts[ii].point->size = W.num_variables;
		
		//NEED TO COPY IN THE WITNESS POINT
		
		//1 set the coordinates
		vec_cp_mp(startPts[ii].point, W.W_mp.pts[ii] );
		
		//2 set the start time to 1.
		set_one_mp(startPts[ii].time);
	}
	
	
	T->endgameOnly = 0;
	
	
  // setup the rest of the structures
	endgame_data_t *EG = NULL;
  setup_detjac_to_detjac_omp_mp(max,
																&EG, &trackCount_copy, trackCount,
																&OUT_copy, OUT, &RAWOUT_copy, RAWOUT, &MIDOUT_copy, MIDOUT, &FAIL_copy, FAIL, &NONSOLN_copy, NONSOLN,
																&T_copy, T,
																&BED_copy, ED);
	
	
	//initialize the structure for holding the produced data
	
	W_new->W.num_pts=0;
  W_new->W.pts=(point_d *)bmalloc(W.W.num_pts*sizeof(point_d));
	
	W_new->W_mp.num_pts=0;
  W_new->W_mp.pts=(point_mp *)bmalloc(W.W_mp.num_pts*sizeof(point_mp));
  W_new->num_variables = W.num_variables;
	
	
	trackCount->numPoints = W.W.num_pts;
	int solution_counter = 0;
	
	
	
	
	
	
	// we pass the particulars of the information for this solve mode via the ED.
	
	
	// track each of the start points
#ifdef _OPENMP
#pragma omp parallel for private(ii, oid, startPointIndex) schedule(runtime)
#endif
	for (ii = 0; ii < W.W_mp.num_pts; ii++)
	{ // get current thread number
		oid = thread_num();
		printf("detjac_to_detjac tracking path %d of %d\n",ii,W.W.num_pts);
		// print the header of the path to OUT
		//			printPathHeader_mp(OUT_copy[oid], &startPts[startPointIndex], &T_copy[oid], solution_counter, &BED_copy[oid], eval_func_mp);
		detjac_to_detjac_track_path_mp(solution_counter, &EG[oid], &startPts[ii], OUT_copy[oid], MIDOUT_copy[oid], &T_copy[oid], &BED_copy[oid], curr_eval_mp, change_prec, find_dehom); //curr_eval_d,
		
		
		
		// check to see if it should be sharpened
		if (EG[oid].retVal == 0 && T_copy[oid].sharpenDigits > 0)
		{ // use the sharpener for after an endgame
			sharpen_endpoint_endgame(&EG[oid], &T_copy[oid], OUT_copy[oid], NULL, &BED_copy[oid], NULL, curr_eval_mp, NULL);
			// DAB - replaced curr_eval_d with NULL
		}
		
		
		if (EG->retVal!=0) {
			printf("\nretVal = %d\nthere was a path failure tracking witness point %d\n",EG->retVal,ii);
			print_path_retVal_message(EG->retVal);
		}
		else
		{
			
			//copy the solutions out of EG.
			
			init_vec_mp(W_new->W_mp.pts[solution_counter],0);
			init_vec_d(W_new->W.pts[solution_counter],0);
			
			
			vec_cp_mp( W_new->W_mp.pts[solution_counter],
								EG->PD_mp.point);
			vec_mp_to_d(W_new->W.pts[solution_counter],
									EG->PD_mp.point)
			solution_counter++;
		}
		
		//			comp_mp result; init_mp(result);
		//			dot_product_mp(result, W_new->W_mp.pts[solution_counter], ED->current_linear);
		//			print_point_to_screen_matlab( W_new->W.pts[solution_counter],"newwitnesspoint");
		//			print_point_to_screen_matlab( ED_d->current_linear,"newlinear");
		//
		//			printf("dot_prod=%le+1i*%le;\n",result->r,result->i);
		
		
		
		
		
		
		
		//				print_point_to_screen_matlab(startPts[startPointIndex].point,"start");
		//				print_point_to_screen_matlab(ED_d->old_linear,"old");
		//
		//				print_point_to_screen_matlab(EG->PD_d.point,"vend"); // the actual solution!!!
		//				print_point_to_screen_matlab(ED_d->current_linear,"curr");
		
		
		//    if (EG[oid].prec < 64)
		//    { // print footer in double precision
		//      printPathFooter_d(&trackCount_copy[oid], &EG[oid], &T_copy[oid], OUT_copy[oid], RAWOUT_copy[oid], FAIL_copy[oid], NONSOLN_copy[oid], &BED_copy[oid]);
		//    }
		//    else
		//    { // print footer in multi precision
		//      printPathFooter_mp(&trackCount_copy[oid], &EG[oid], &T_copy[oid], OUT_copy[oid], RAWOUT_copy[oid], FAIL_copy[oid], NONSOLN_copy[oid], BED_copy[oid].BED_mp);
		//    }
		
		
	}// re: for (ii=0; ii<W.W.num_pts ;ii++)
	
	
	W_new->W.num_pts=solution_counter;
	printf("found %d solutions after the detjac_to_detjac regeneration step\n",solution_counter);
	
	
	
	
	
	
	
	//clear the data structures.
	
  for (ii = 0; ii >W.W_mp.num_pts; ii++)
  { // clear startPts[ii]
    clear_point_data_mp(&startPts[ii]);
  }
  free(startPts);
	
  // clear the structures
  clear_detjactodetjac_omp_mp(max, &EG, &trackCount_copy, trackCount, &OUT_copy, OUT, &RAWOUT_copy, RAWOUT, &MIDOUT_copy, MIDOUT, &FAIL_copy, FAIL, &NONSOLN_copy, NONSOLN, &T_copy, &BED_copy);
	
  return;
}



//int (*eval_func_d)(point_d, point_d, vec_d, mat_d, mat_d, point_d, comp_d, void const *),

// derived from zero_dim_track_path_d
void detjac_to_detjac_track_path_mp(int pathNum, endgame_data_t *EG_out,
																		point_data_mp *Pin,
																		FILE *OUT, FILE *MIDOUT, tracker_config_t *T,
																		void const *ED,
																		int (*eval_func_mp)(point_mp, point_mp, vec_mp, mat_mp, mat_mp, point_mp, comp_mp, void const *),
																		int (*change_prec)(void const *, int),
																		int (*find_dehom)(point_d, point_mp, int *, point_d, point_mp, int, void const *, void const *))
/***************************************************************\
 * USAGE:                                                        *
 * ARGUMENTS:                                                    *
 * RETURN VALUES:                                                *
 * NOTES: actually does the zero-dimensional tracking and sets   *
 *  up EG_out                                                    *
 \***************************************************************/
{
	
	EG_out->pathNum = pathNum;
  EG_out->codim = 0; // zero dimensional - this is ignored
	
  T->first_step_of_path = 1;
	
  // track using MP
  EG_out->retVal = endgame_mp(T->endgameNumber, EG_out->pathNum, &EG_out->PD_mp, EG_out->last_approx_mp, Pin, T, OUT, MIDOUT, ED, eval_func_mp, find_dehom);
	
	
  EG_out->prec = EG_out->last_approx_prec = T->Precision;
  EG_out->first_increase = 0;
	
  // copy over the values
  mpf_set(EG_out->latest_newton_residual_mp, T->latest_newton_residual_mp);
  mpf_set_d(EG_out->t_val_at_latest_sample_point_mp, T->t_val_at_latest_sample_point);
  mpf_set_d(EG_out->error_at_latest_sample_point_mp, T->error_at_latest_sample_point);
  findFunctionResidual_conditionNumber_mp(EG_out->function_residual_mp, &EG_out->condition_number, &EG_out->PD_mp, ED, eval_func_mp);
	
  return;
}




// derived from zero_dim_basic_setup_d
int detjac_to_detjac_setup_mp(FILE **OUT, char *outName,
															FILE **midOUT, char *midName,
															tracker_config_t *T,
															detjactodetjac_eval_data_mp *ED,
															prog_t *dummyProg,
															int **startSub, int **endSub, int **startFunc, int **endFunc, int **startJvsub, int **endJvsub, int **startJv, int **endJv, int ***subFuncsBelow,
															int (**eval_d)(point_d, point_d, vec_d, mat_d, mat_d, point_d, comp_d, void const *),
															int (**eval_mp)(point_mp, point_mp, vec_mp, mat_mp, mat_mp, point_mp, comp_mp, void const *),
															char *preprocFile, char *degreeFile,
															int findStartPts, char *pointsIN, char *pointsOUT,
															mat_mp n_minusone_randomizer_matrix,
															witness_set_d W,
															vec_mp old_projection_full_prec,
															vec_mp new_projection_full_prec)
/***************************************************************\
 * USAGE:                                                        *
 * ARGUMENTS:                                                    *
 * RETURN VALUES: number of original variables                   *
 * NOTES: setup for zero dimensional tracking                    *
 \***************************************************************/
{ // need to create the homotopy
	
  int rank, patchType, ssType, numOrigVars, adjustDegrees, numGps;
	
  *eval_d = &detjac_to_detjac_eval_d;
  *eval_mp = &detjac_to_detjac_eval_mp;
	
  *OUT = fopen(outName, "w");  // open the main output files.
  *midOUT = fopen(midName, "w");
	
	
	
	
  // setup a straight-line program, using the file(s) created by the parser
  T->numVars = numOrigVars = setupProg_count(dummyProg, T->Precision, T->MPType, startSub, endSub, startFunc, endFunc, startJvsub, endJvsub, startJv, endJv, subFuncsBelow);
	
	
  // setup preProcData
  setupPreProcData(preprocFile, &ED->preProcData);
	
	
	
  numGps = ED->preProcData.num_var_gp + ED->preProcData.num_hom_var_gp;
  // find the rank
	//  rank = rank_finder_mp(&ED->preProcData, dummyProg, T, T->numVars);
	//  // check to make sure that it is possible to have a zero dimensional component
	//  if (T->numVars > rank + numGps)
	//		//  {
	//		//    printf("The system has no zero dimensional solutions based on its rank!\n");
	//		//    printf("The rank of the system including the patches is %d while the total number of variables is %d.\n\n", rank + numGps, T->numVars);
	//		//    bexit(ERROR_INPUT_SYSTEM);
	//		//  }
	//
	//		//AM I ACUTALLY SUPPOSED TO DO THIS? !!! HERE
	//		// adjust the number of variables based on the rank
	//		T->numVars = rank + numGps;
	
	
	
	
  // now that we know the rank, we can setup the rest of ED
	//  if (numGps == 1)// this should ALWAYS be the case in this solver.
	//  { // 1-hom
	patchType = 2; // 1-hom patch
	ssType = 0;    // with 1-hom, we use total degree start system
	adjustDegrees = 0; // if the system does not need its degrees adjusted, then that is okay
	setupdetjactodetjacEval_mp(preprocFile, degreeFile, dummyProg, rank, patchType, ssType, T->Precision, &T->numVars, NULL, NULL, NULL, ED, adjustDegrees, n_minusone_randomizer_matrix, W,old_projection_full_prec,new_projection_full_prec);
	//  }
	//  else
	//  { // m-hom, m > 1
	//    patchType = 0; // random patch based on m-hom variable structure
	//    ssType = 1;    // with m-hom, we use the mhom structure for start system
	//    adjustDegrees = 0; // if the system does not need its degrees adjusted, then that is okay
	//    setupdetjactodetjacEval_mp(preprocFile, degreeFile, dummyProg, rank, patchType, ssType, T->Precision, &ED->preProcData, NULL, NULL, NULL, ED, adjustDegrees, n_minusone_randomizer_matrix, W,);
	//  }
	
	
  return numOrigVars;
}





//this derived from basic_eval_d
int detjac_to_detjac_eval_mp(point_mp funcVals, point_mp parVals, vec_mp parDer, mat_mp Jv, mat_mp Jp, point_mp current_variable_values, comp_mp pathVars, void const *ED)
{ // evaluates a special homotopy type, built for bertini_real
//	printf("entering detjacdetjac_eval_mp\n");
  detjactodetjac_eval_data_mp *BED = (detjactodetjac_eval_data_mp *)ED; // to avoid having to cast every time
	
	int ii, jj, kk;
  comp_mp one_minus_s, gamma_s; init_mp(one_minus_s); init_mp(gamma_s);
	comp_mp detjac_old;  init_mp(detjac_old);
	comp_mp detjac_new;  init_mp(detjac_new);
	comp_mp temp, temp2; init_mp(temp); init_mp(temp2);
	
	
	set_one_mp(one_minus_s);
  sub_mp(one_minus_s, one_minus_s, pathVars);  // one_minus_s = (1 - s)
  mul_mp(gamma_s, BED->gamma, pathVars);       // gamma_s = gamma * s
	
  // we assume that the only parameter is s = t and setup parVals & parDer accordingly.
	
	
	
	vec_mp patchValues; init_vec_mp(patchValues, 0);
	vec_mp temp_function_values; init_vec_mp(temp_function_values,0);
	vec_mp AtimesF;  init_vec_mp(AtimesF,0);
	
	
	mat_mp Jv_Patch; init_mat_mp(Jv_Patch, 0, 0);
	mat_mp tempmat; init_mat_mp(tempmat,0,0);
	mat_mp AtimesJ; init_mat_mp(AtimesJ,1,1);
	mat_mp Jv_detjac_old; init_mat_mp(Jv_detjac_old,0,0);
	mat_mp Jv_detjac_new; init_mat_mp(Jv_detjac_new,0,0);
	mat_mp temp_jacobian_functions, temp_jacobian_parameters; init_mat_mp(temp_jacobian_functions,0,0); init_mat_mp(temp_jacobian_parameters,0,0);
	
	
	
	

	
	
	
	evalProg_mp(temp_function_values, parVals, parDer, temp_jacobian_functions, temp_jacobian_parameters, current_variable_values, pathVars, BED->SLP);
	
	
  // evaluate the patch
  patch_eval_mp(    patchValues, parVals, parDer, Jv_Patch, Jp, current_variable_values, pathVars, &BED->patch);  // Jp is ignored
	
	
	
  // combine everything
	
	change_size_vec_mp(funcVals,BED->num_variables);
  change_size_mat_mp(Jv, BED->num_variables, BED->num_variables);
  change_size_mat_mp(Jp, BED->num_variables, 1);
	
	
  funcVals->size = Jv->rows = Jp->rows = BED->num_variables;
  Jv->cols = BED->num_variables;  //  -> this should be square!!!
  Jp->cols = 1;
	
	mat_mul_mp(AtimesJ,BED->n_minusone_randomizer_matrix,temp_jacobian_functions);
	
	
	mul_mat_vec_mp(AtimesF,BED->n_minusone_randomizer_matrix, temp_function_values ); // set values of AtimesF (A is randomization matrix)
	
	for (ii=0; ii<AtimesF->size; ii++) { // for each function, after (real orthogonal) randomization
		set_mp(&funcVals->coord[ii], &AtimesF->coord[ii]);
	}
	
	
	
	
	
	
	
	
	
	//the determinant of the jacobian, with the projection.
	
	mat_cp_mp(tempmat,AtimesJ);// copy into the matrix of which we will take the determinant
	
	increase_size_mat_mp(tempmat,BED->num_variables,BED->num_variables); // make it bigger to accomodate more entries.  make square
	tempmat->rows = tempmat->cols = BED->num_variables; // change the size indicators
	
	//copy in the jacocian of the patch equation
	for (ii=0; ii<BED->num_variables; ++ii) {
		set_mp(&tempmat->entry[BED->num_variables-1][ii],&Jv_Patch->entry[0][ii]); //  copy in the patch jacobian
	}
	
	
	//copy in the OLD projection from BED
	for (ii=0; ii<BED->num_variables; ++ii) {
		set_mp(&tempmat->entry[BED->num_variables-2][ii],&BED->old_projection->coord[ii]); // copy in the old_projection
	}
	//now TAKE THE DETERMINANT of tempmat.
	//	print_matrix_to_screen_matlab(tempmat,"det1");
	take_determinant_mp(detjac_old,tempmat); // the determinant goes into detjac
	mul_mp(temp,detjac_old,gamma_s);
	
	
	
	//copy in the NEW projection from BED
	for (ii=0; ii<BED->num_variables; ++ii) {
		set_mp(&tempmat->entry[BED->num_variables-2][ii],&BED->new_projection->coord[ii]); // copy in the old_projection
	}
	//now TAKE THE DETERMINANT of tempmat.
	//	print_matrix_to_screen_matlab(tempmat,"det2");
	take_determinant_mp(detjac_new,tempmat); // the determinant goes into detjac
	mul_mp(temp2,detjac_new,one_minus_s);
	
	
	add_mp(&funcVals->coord[BED->num_variables-2],temp,temp2);  // (1-s)detjac_new + gamma*s detjac_old

	
	
	//set the PATCH values
	int offset = BED->num_variables-1;
	for (ii=0; ii<BED->patch.num_patches; ii++) {
		set_mp(&funcVals->coord[ii+offset], &patchValues->coord[ii]);
	}
	
	
	
	
	
	//now for the NUMERICAL DERIVATIVE.
	
	detjac_numerical_derivative_mp(Jv_detjac_old, // return value
																current_variable_values, pathVars, BED->old_projection,
																BED->num_variables,
																BED->patch,
																BED->SLP,
																BED->n_minusone_randomizer_matrix); // input parameters for the method
	
	detjac_numerical_derivative_mp(Jv_detjac_new, // return value
																current_variable_values, pathVars, BED->new_projection,
																BED->num_variables,
																BED->patch,
																BED->SLP,
																BED->n_minusone_randomizer_matrix); // input parameters for the method
	

	
	//////////////
	//
	// SET THE FINAL RETURNED JACOBIAN ENTRIES.
	//
	////////////////////
	// note: matrix was sized appropriately above.
	
	
	//first, the entries related to the functions
  for (ii = 0; ii < BED->num_variables-2; ii++)
  {
		for (jj = 0; jj < BED->num_variables; jj++)
		{
			set_mp(&Jv->entry[ii][jj],&AtimesJ->entry[ii][jj]);
		}
  }
	
	
	
	
	
	// the entries in the jacobian for the detjac terms.
	
	
	
	offset = BED->num_variables-2;
	for (ii=0; ii<BED->num_variables; ii++) {
		mul_mp(temp, &Jv_detjac_old->entry[0][ii],gamma_s); // temp = d/dx detjac_old * gamma * s
		mul_mp(temp2,&Jv_detjac_new->entry[0][ii],one_minus_s);// temp2 = d/dx detjac_new * (1-s)
		add_mp(&Jv->entry[offset][ii],temp, temp2);  // Jv = (1-s)d/dx(detjac) + s*gamma*d/dx detjac
	}
	//&&&&&&&&&&&
	
	
	// the last entries come from the patch equation(s)
	offset = BED->num_variables-1;
	for (ii=0; ii<BED->num_variables; ii++) {
		set_mp(&Jv->entry[offset][ii],&Jv_Patch->entry[0][ii]);
	}
	
	
	for (ii = 0; ii<BED->num_variables-2; ii++) {
		set_zero_mp(&Jp->entry[ii][0]);  // no parameter dependence means zero derivative for these functions
	}
	
	
	//initialize
	//Jp[N-2][0] = -detjac_new + gamma*detjac_old
	
	
	set_mp(&Jp->entry[BED->num_variables-2][0],detjac_new);
	neg_mp(&Jp->entry[BED->num_variables-2][0],&Jp->entry[BED->num_variables-2][0]);
	
	mul_mp(temp,BED->gamma,detjac_old);  // gamma * detjac_old
	add_mp(&Jp->entry[BED->num_variables-2][0],&Jp->entry[BED->num_variables-2][0],temp);
	
	
	
	
	// the entries in the jacobian for the patch equations.
	offset = BED->num_variables-1;
	for (ii = 0; ii<BED->patch.num_patches; ii++)  // for each patch equation
	{ // funcVals = patchValues
		// Jp = 0
		set_zero_mp(&Jp->entry[ii+offset][0]);
		// Jv = Jv_Patch
		for (jj = 0; jj<BED->num_variables; jj++) // for each variable
		{
			set_mp(&Jv->entry[ii+offset][jj], &Jv_Patch->entry[ii][jj]);
		}
		
	}
	
	
	
	
	// set parVals & parDer correctly
  change_size_point_mp(parVals, 1);
  change_size_vec_mp(parDer, 1);
	
  parVals->size = parDer->size = 1;
  set_mp(&parVals->coord[0], pathVars); // s = t
  set_one_mp(&parDer->coord[0]);       // ds/dt = 1
	
	
	
//	print_matrix_to_screen_matlab( AtimesJ,"jac");
//	print_point_to_screen_matlab(current_variable_values,"currvars");
//print_comp_mp_matlab(pathVars,"pathvars");


//	print_matrix_to_screen_matlab(Jv_detjac,"Jv_detjac");
//	print_point_to_screen_matlab_mp(funcVals,"F");
//	print_point_to_screen_matlab(parVals,"parVals");
//	print_point_to_screen_matlab(parDer,"parDer");
//	print_matrix_to_screen_matlab_mp(Jv,"Jv");
//	print_matrix_to_screen_matlab(Jp,"Jp");


	//these values are set in this function:  point_mp funcVals, point_mp parVals, vec_mp parDer, mat_mp Jv, mat_mp Jp
//	print_matrix_to_screen_matlab(BED->n_minusone_randomizer_matrix,"n_minusone_randomizer_matrix");
//
//	mypause();
//
	
	
	clear_mp(one_minus_s);
	clear_mp(gamma_s);
	clear_mp(detjac_old);
	clear_mp(detjac_new);
	clear_mp(temp);
	clear_mp(temp2);

	clear_mat_mp(temp_jacobian_functions);
	clear_mat_mp(temp_jacobian_parameters);
	clear_mat_mp(AtimesJ);
	clear_mat_mp(tempmat);
	clear_mat_mp(Jv_detjac_new);
	clear_mat_mp(Jv_detjac_old);
	clear_mat_mp(Jv_Patch);
	
	clear_vec_mp(patchValues);
	clear_vec_mp(temp_function_values);
	clear_vec_mp(AtimesF);
//
//	BED=NULL;
//	
//	//	printf("exiting eval\n");
  return 0;
} //re: evaluator mp







void detjactodetjac_eval_clear_mp(detjactodetjac_eval_data_mp *ED, int clearRegen, int MPType)
/***************************************************************\
 * USAGE:                                                        *
 * ARGUMENTS:                                                    *
 * RETURN VALUES:                                                *
 * NOTES: clear ED                                               *
 \***************************************************************/
{
	
  patch_eval_data_clear_mp(&ED->patch);
  preproc_data_clear(&ED->preProcData);
	
	
	//TODO: SOME OTHER THINGS OUGHT TO BE CLEARED HERE  DAB  and in the corresponding place in the _d function
	
	
	//specifics for the detjactodetjac method.
	clear_mp(ED->gamma);
	clear_vec_mp(ED->old_projection);
	clear_vec_mp(ED->new_projection);
	clear_mat_mp(ED->n_minusone_randomizer_matrix);
	
	
	
  return;
}






void setup_detjac_to_detjac_omp_mp(int max_threads, endgame_data_t **EG, trackingStats **trackCount_copy, trackingStats *trackCount,
																	 FILE ***OUT_copy, FILE *OUT, FILE ***RAWOUT_copy, FILE *RAWOUT,
																	 FILE ***MIDOUT_copy, FILE *MIDOUT, FILE ***FAIL_copy, FILE *FAIL,
																	 FILE ***NONSOLN_copy, FILE *NONSOLN,
																	 tracker_config_t **T_copy, tracker_config_t *T,
																	 detjactodetjac_eval_data_mp **BED_copy, detjactodetjac_eval_data_mp *ED_mp)
/***************************************************************\
 * USAGE:                                                        *
 * ARGUMENTS:                                                    *
 * RETURN VALUES:                                                *
 * NOTES: setup everything needed to do zero dimensional tracking*
 *  using OpenMP                                                 *
 \***************************************************************/
// if max_threads == 1, things are only pointers to the actual values,
// otherwise, they are copies
{
  int ii;
  // error checking
  if (max_threads <= 0)
  {
    printf("\n\nERROR: The number of threads (%d) needs to be positive when setting up for tracking!\n", max_threads);
    bexit(ERROR_CONFIGURATION);
  }
	
	
	// allocate space for EG
	*EG = (endgame_data_t *)bmalloc(max_threads * sizeof(endgame_data_t));
  for (ii = 0; ii < max_threads; ii++)
  {
    init_endgame_data(&(*EG)[ii], T->Precision);
  }
	
	
	//  *EG = (endgame_data_t *)bmalloc(max_threads * sizeof(endgame_data_t));
	//
	//	// initialize
	//  for (ii = 0; ii < max_threads; ii++)
	//	{
	//    if (T->MPType == 2)
	//    { // initialize for AMP tracking
	//      init_endgame_data(&(*EG)[ii], 64);
	//    }
	//    else
	//    { // initialize for double precision tracking
	//      init_endgame_data(&(*EG)[ii], 52);
	//    }
	//	}
	
  // allocate space to hold pointers to the files
  *OUT_copy = (FILE **)bmalloc(max_threads * sizeof(FILE *));
  *MIDOUT_copy = (FILE **)bmalloc(max_threads * sizeof(FILE *));
  *RAWOUT_copy = (FILE **)bmalloc(max_threads * sizeof(FILE *));
  *FAIL_copy = (FILE **)bmalloc(max_threads * sizeof(FILE *));
  *NONSOLN_copy = (FILE **)bmalloc(max_threads * sizeof(FILE *));
	
  if (max_threads == 1)
  { // setup the pointers
    *trackCount_copy = trackCount;
    *T_copy = T;
    *BED_copy = ED_mp;
		//    (*BED_copy)->BED_mp = ED_mp; // make sure that this is pointed to inside of ED_d
		
    (*OUT_copy)[0] = OUT;
    (*RAWOUT_copy)[0] = RAWOUT;
    (*MIDOUT_copy)[0] = MIDOUT;
    (*FAIL_copy)[0] = FAIL;
    (*NONSOLN_copy)[0] = NONSOLN;
  }
  else // max_threads > 1
  { // allocate memory
		printf("more than one thread? prolly gonna crap out here in a sec\n");
		mypause();
		
    *trackCount_copy = (trackingStats *)bmalloc(max_threads * sizeof(trackingStats));
    *T_copy = (tracker_config_t *)bmalloc(max_threads * sizeof(tracker_config_t));
    *BED_copy = (detjactodetjac_eval_data_mp *)bmalloc(max_threads * sizeof(detjactodetjac_eval_data_mp));
		
    // copy T, ED_d, ED_mp, & trackCount
    for (ii = 0; ii < max_threads; ii++)
    { // copy T
      cp_tracker_config_t(&(*T_copy)[ii], T);
      // copy ED_d & ED_mp
      cp_detjactodetjac_eval_data_mp(&(*BED_copy)[ii], ED_mp, T->MPType);
      // initialize trackCount_copy
      init_trackingStats(&(*trackCount_copy)[ii]);
      (*trackCount_copy)[ii].numPoints = trackCount->numPoints;
    }
		
    // setup the files
    char *str = NULL;
    int size;
    for (ii = 0; ii < max_threads; ii++)
    {
      size = 1 + snprintf(NULL, 0, "output_%d", ii);
      str = (char *)brealloc(str, size * sizeof(char));
      sprintf(str, "output_%d", ii);
      (*OUT_copy)[ii] = fopen(str, "w+");
			
      size = 1 + snprintf(NULL, 0, "midout_%d", ii);
      str = (char *)brealloc(str, size * sizeof(char));
      sprintf(str, "midout_%d", ii);
      (*MIDOUT_copy)[ii] = fopen(str, "w+");
			
      size = 1 + snprintf(NULL, 0, "rawout_%d", ii);
      str = (char *)brealloc(str, size * sizeof(char));
      sprintf(str, "rawout_%d", ii);
      (*RAWOUT_copy)[ii] = fopen(str, "w+");
			
      size = 1 + snprintf(NULL, 0, "fail_%d", ii);
      str = (char *)brealloc(str, size * sizeof(char));
      sprintf(str, "fail_%d", ii);
      (*FAIL_copy)[ii] = fopen(str, "w+");
			
      size = 1 + snprintf(NULL, 0, "nonsolutions_%d", ii);
      str = (char *)brealloc(str, size * sizeof(char));
      sprintf(str, "nonsolutions_%d", ii);
      (*NONSOLN_copy)[ii] = fopen(str, "w+");
    }
    free(str);
  }
	
	
  return;
}

void clear_detjactodetjac_omp_mp(int max_threads, endgame_data_t **EG, trackingStats **trackCount_copy, trackingStats *trackCount, FILE ***OUT_copy, FILE *OUT, FILE ***RAWOUT_copy, FILE *RAWOUT, FILE ***MIDOUT_copy, FILE *MIDOUT, FILE ***FAIL_copy, FILE *FAIL, FILE ***NONSOLN_copy, FILE *NONSOLN, tracker_config_t **T_copy, detjactodetjac_eval_data_mp **BED_copy)
/***************************************************************\
 * USAGE:                                                        *
 * ARGUMENTS:                                                    *
 * RETURN VALUES:                                                *
 * NOTES: copy the relevant data back to the standard spot and   *
 *  clear the allocated data that was used by OpenMP             *
 \***************************************************************/
// if max_threads == 1, things are only pointers to the actual values,
// otherwise, they are copies
{
  int ii;
	
  // clear EG
  for (ii = max_threads - 1; ii >= 0; ii--)
  {
    clear_endgame_data(&(*EG)[ii]);
  }
  free(*EG);
	
  if (max_threads == 1)
  { // set the pointers to NULL since they just pointed to the actual values
    *trackCount_copy = NULL;
    *T_copy = NULL;
    *BED_copy = NULL;
		
    *OUT_copy[0] = NULL;
    *RAWOUT_copy[0] = NULL;
    *MIDOUT_copy[0] = NULL;
    *FAIL_copy[0] = NULL;
		
    // free the memory of the file pointers
    free(*OUT_copy);
    free(*MIDOUT_copy);
    free(*RAWOUT_copy);
    free(*FAIL_copy);
    free(*NONSOLN_copy);
  }
  else if (max_threads > 1)
  {
    // combine trackCount_copy
    add_trackingStats(trackCount, *trackCount_copy, max_threads);
		
    // clear the copies T, ED_d & ED_mp
    for (ii = max_threads - 1; ii >= 0; ii--)
    { // clear BED_copy - 0 since not using regeneration
      detjactodetjac_eval_clear_mp(&(*BED_copy)[ii], 0, (*T_copy)[ii].MPType);
			// clear T_copy
      tracker_config_clear(&(*T_copy)[ii]);
    }
		
    // free the memory
    free(*trackCount_copy);
    free(*T_copy);
    free(*BED_copy);
		
    // copy all of the files to the appropriate place
    char ch, *str = NULL;
    int size;
    for (ii = 0; ii < max_threads; ii++)
    {
      // rewind to beginning for reading
      rewind((*OUT_copy)[ii]);
      // copy over to OUT
      ch = fgetc((*OUT_copy)[ii]);
      while (ch != EOF)
      {
        fprintf(OUT, "%c", ch);
        ch = fgetc((*OUT_copy)[ii]);
      }
      // close file & delete
      fclose((*OUT_copy)[ii]);
      size = 1 + snprintf(NULL, 0, "output_%d", ii);
      str = (char *)brealloc(str, size * sizeof(char));
      sprintf(str, "output_%d", ii);
      remove(str);
			
      // rewind to beginning for reading
      rewind((*MIDOUT_copy)[ii]);
      // copy over to MIDOUT
      ch = fgetc((*MIDOUT_copy)[ii]);
      while (ch != EOF)
      {
        fprintf(MIDOUT, "%c", ch);
        ch = fgetc((*MIDOUT_copy)[ii]);
      }
      // close file & delete
      fclose((*MIDOUT_copy)[ii]);
      size = 1 + snprintf(NULL, 0, "midout_%d", ii);
      str = (char *)brealloc(str, size * sizeof(char));
      sprintf(str, "midout_%d", ii);
      remove(str);
			
      // rewind to beginning for reading
      rewind((*RAWOUT_copy)[ii]);
      // copy over to RAWOUT
      ch = fgetc((*RAWOUT_copy)[ii]);
      while (ch != EOF)
      {
        fprintf(RAWOUT, "%c", ch);
        ch = fgetc((*RAWOUT_copy)[ii]);
      }
      // close file & delete
      fclose((*RAWOUT_copy)[ii]);
      size = 1 + snprintf(NULL, 0, "rawout_%d", ii);
      str = (char *)brealloc(str, size * sizeof(char));
      sprintf(str, "rawout_%d", ii);
      remove(str);
			
      // rewind to beginning for reading
      rewind((*FAIL_copy)[ii]);
      // copy over to FAIL
      ch = fgetc((*FAIL_copy)[ii]);
      while (ch != EOF)
      {
        fprintf(FAIL, "%c", ch);
        ch = fgetc((*FAIL_copy)[ii]);
      }
      // close file & delete
      fclose((*FAIL_copy)[ii]);
      size = 1 + snprintf(NULL, 0, "fail_%d", ii);
      str = (char *)brealloc(str, size * sizeof(char));
      sprintf(str, "fail_%d", ii);
      remove(str);
			
      // rewind to beginning for reading
      rewind((*NONSOLN_copy)[ii]);
      // copy over to NONSOLN
      ch = fgetc((*NONSOLN_copy)[ii]);
      while (ch != EOF)
      {
        fprintf(NONSOLN, "%c", ch);
        ch = fgetc((*NONSOLN_copy)[ii]);
      }
      // close file & delete
      fclose((*NONSOLN_copy)[ii]);
      size = 1 + snprintf(NULL, 0, "nonsolutions_%d", ii);
      str = (char *)brealloc(str, size * sizeof(char));
      sprintf(str, "nonsolutions_%d", ii);
      remove(str);
    }
    free(str);
    // free file memory
    free(*OUT_copy);
    free(*MIDOUT_copy);
    free(*RAWOUT_copy);
    free(*FAIL_copy);
    free(*NONSOLN_copy);
  }
	
  return;
}




void setupdetjactodetjacEval_mp(char preprocFile[], char degreeFile[], prog_t *dummyProg,
																int squareSize, int patchType, int ssType, int prec,
																void const *ptr1, void const *ptr2, void const *ptr3, void const *ptr4,
																detjactodetjac_eval_data_mp *BED, int adjustDegrees,
																mat_mp n_minusone_randomizer_matrix,
																witness_set_d W,
																vec_mp old_projection_full_prec,
																vec_mp new_projection_full_prec)
{
  int ii;
	int digits = prec_to_digits(mpf_get_default_prec());
  setupPreProcData(preprocFile, &BED->preProcData);
	
	setupPatch_mp(patchType, &BED->patch, digits, prec, ptr1, ptr2);
	//
	
	printf("setup linprod_mp 3237 \n");
	BED->num_variables = W.num_variables;
	
	
	//	print_matrix_to_screen_matlab(BED->patch.patchCoeff,"stockpatch");
	//	print_point_to_screen_matlab(
	//	printf("%d\n",W.patch_size);
	
	for (ii = 0; ii < BED->num_variables ; ii++)
	{
		set_mp(&BED->patch.patchCoeff->entry[0][ii],&W.patch_mp[0]->coord[ii]);
	}
	
	//	print_matrix_to_screen_matlab(BED->patch.patchCoeff,"userpatch");
	
	
	//	vec_mp patchValues;
	//	init_vec_mp(patchValues,0);
	//	point_mp parVals;
	//	init_vec_mp(parVals,0);
	//	vec_mp parDer;
	//	init_vec_mp(parDer,0);
	//	mat_mp Jv_Patch;
	//	init_mat_mp(Jv_Patch,0,0);
	//	mat_mp Jp;
	//	init_mat_mp(Jp,0,0);
	//	comp_mp pathVars; init_mp(pathVars);
	//	set_one_mp(pathVars);
	//	patch_eval_mp(    patchValues, parVals, parDer, Jv_Patch, Jp, W.W_mp.pts[0], pathVars, &BED->patch);  // Jp is ignored
	//
	//	print_point_to_screen_matlab_mp(patchValues,"patchvalues");
	//	print_point_to_screen_matlab_mp(W.W_mp.pts[0],"initialpoint");
	//	mypause();
	
	
	BED->SLP = dummyProg;
	
	
	init_mat_mp(BED->n_minusone_randomizer_matrix,0,0);
	mat_cp_mp(BED->n_minusone_randomizer_matrix,
						n_minusone_randomizer_matrix);
	
	
	// set up the vectors to hold the two linears.
	
	
	init_vec_mp(BED->old_projection,BED->num_variables); BED->old_projection->size = BED->num_variables;
	vec_cp_mp(BED->old_projection,old_projection_full_prec);
	
	
	init_vec_mp(BED->new_projection,BED->num_variables); BED->new_projection->size = BED->num_variables;
	vec_cp_mp(BED->new_projection,new_projection_full_prec);
	
	
	
	init_mp2(BED->gamma,prec);
	get_comp_rand_mp(BED->gamma); // set gamma to be random complex value
	
	

	
	printf("exit setup_mp\n");
	
  return;
}



void cp_detjactodetjac_eval_data_mp(detjactodetjac_eval_data_mp *BED, detjactodetjac_eval_data_mp *BED_mp_input, int MPType)
/***************************************************************\
 * USAGE:                                                        *
 * ARGUMENTS:                                                    *
 * RETURN VALUES:                                                *
 * NOTES: stores a copy of BED_(t)_input to BED                  *
 \***************************************************************/
{
	printf("entering cp_detjactodetjac_eval_data_mp\nthis function is likely broken\n");
	exit(-1);
  cp_preproc_data(&BED->preProcData, &BED_mp_input->preProcData);
	//  cp_square_system_d(&BED->squareSystem, &BED_d_input->squareSystem);
  cp_patch_mp(&BED->patch, &BED_mp_input->patch);
	//  cp_start_system_d(&BED->startSystem, &BED_d_input->startSystem);
	BED->SLP = (prog_t *)bmalloc(1 * sizeof(prog_t));
  cp_prog_t(BED->SLP, BED_mp_input->SLP);
	
	
	//HERE COPY THE MATRICES  DAB !!!
	init_mat_mp(BED->n_minusone_randomizer_matrix,0,0);
	mat_cp_mp(BED->n_minusone_randomizer_matrix,BED_mp_input->n_minusone_randomizer_matrix);
	
	set_mp(BED->gamma, BED_mp_input->gamma);
	
	
  return;
}





































//



