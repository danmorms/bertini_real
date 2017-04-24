#include "sampler.hpp"









////////////////
//
//  High level curve samplers.  Edge samplers below these.
//
//////////////



void Curve::AdaptiveMovementSampler(VertexSet & V,
										   sampler_configuration & sampler_options,
										   SolverConfiguration & solve_options)
{

	adaptive_set_initial_sample_data();

	std::cout << "adaptively refining curve with " << num_edges() << " edges by adaptive-movement method" << std::endl;
	
	for (unsigned int ii=0; ii<num_edges(); ii++) // for each of the edges
	{
        SampleEdgeAdaptiveMovement(ii, V, sampler_options, solve_options);
	}  // re: ii (for each edge)
	
}





void Curve::AdaptiveDistanceSampler(VertexSet & V,
													sampler_configuration & sampler_options,
													SolverConfiguration & solve_options)
{
    
	adaptive_set_initial_sample_data();

	std::cout << "adaptively refining curve with " << num_edges() << " edges by distance-movement method" << std::endl;

	if (sampler_options.verbose_level()>=1)
		std::cout << "sampling curve with " << num_edges() << " edges " << std::endl;
	
	
	for (unsigned int ii=0; ii<num_edges(); ii++) // for each of the edges
	{
        SampleEdgeAdaptiveDistance(ii, V, sampler_options, solve_options);
	}  // re: ii (for each edge)
	
	
	
}




void Curve::SemiFixedSampler(VertexSet & V,
									  sampler_configuration & sampler_options,
									  SolverConfiguration & solve_options,
									  std::vector<int> const& num_samples_per_interval)
{
	semi_fixed_set_initial_sample_data(num_samples_per_interval, V);

	for (unsigned int ii=0; ii<num_edges(); ii++) // for each of the edges
	{
		if (get_edge(ii).is_degenerate()) 
			continue;

		if (sampler_options.verbose_level() >= 1)
			std::cout << "\tsampling edge " << ii << std::endl;

		SampleEdgeSemiFixed(ii,V,sampler_options, solve_options, num_samples_per_interval);
		
	}  // re: ii (for each edge)
    
}


void Curve::FixedSamplerSerial()
{
	fixed_set_initial_sample_data(target_num_samples);

	for (unsigned int ii=0; ii<num_edges(); ii++) // for each of the edges
	{
		if (get_edge(ii).is_degenerate()) 
			continue;
		
		if (sampler_options.verbose_level() >= 1)
			std::cout << "\tsampling edge " << ii << std::endl;

		SampleEdgeFixed(ii,V, sampler_options, solve_options, target_num_samples);
	
	}  // re: ii (for each edge)
}



void WorkerSampleCurve(sampler_configuration & sampler_options, SolverConfiguration & solve_options)
{
	// get call for help (happened a level above in worker sampler in sampler.cpp (~455))

	// get vertex set
	// get curve
	VertexSet V;
	V.set_tracker_config(&solve_options.T);
	V.receive(solve_options.head(), solve_options);


	Curve C;
	C.receive(solve_options.head(), solve_options);

	switch (sampler_options.mode)
	{
		case sampler_configuration::Mode::Fixed:
		{
			FixedSamplerWorker(V, sampler_options, solve_options); // gets number of samples per edge IN this call
			break;
		}
		default:
			std::cout << color::red() << "sorry, not implemented in parallel yet\n\n" << color::console_default();
		// two (three) more cases for later
	}
	
}


void Curve::FixedSamplerMaster(VertexSet & V,
									  sampler_configuration & sampler_options,
									  SolverConfiguration & solve_options,
									  int target_num_samples)
{
	solve_options.call_for_help(CURVE_SAMPLE); // sets available workers, too

	// send all workers 
	//     • vertex set
	//     • curve decomposition
	//     • which sampling method to use  
	//        MPI_Bcast(sampler_options.mode..., solve_options.head(), solve_options.comm());
	//seed the workers
	for (int ii=1; ii<solve_options.num_procs(); ii++) {
		V.send(ii, solve_options);
		this->send(ii, solve_options); // `this` is the curve
		MPI_Send(&target_num_samples, 1, MPI_INT, ii, 10, solve_options.comm());
	}


	// send workers target_num_samples

	for (int ii=0; ii < num_edges(); ++ii)
	{
		int next_worker = solve_options.activate_next_worker();
		MPI_Send(&ii, 1, MPI_INT, next_worker, 10, solve_options.comm());
		//send positive edge_index to worker

		if (solve_options.have_available())
			continue;

		ReportEdgeMaster();
	}


	//wait for everybody to finish

	while (solve_options.have_active()) {// each active worker
		// receive note from worker ___ that edge ___ is done
		// synchronize curves, vertex set
		// send negative edge_index 
	}

}


void Curve::FixedSamplerWorker(VertexSet & V,
									  sampler_configuration & sampler_options,
									  SolverConfiguration & solve_options)
{
	int target_num_samples;
	MPI_Recv(&target_num_samples, 1, MPI_INT, solve_options.head(), 10, solve_options.comm(), MPI_STATUS_UNUSED);

	while (1) // while there is work to be done (an edge to sample)
	{
		int edge_index;
		MPI_Recv(&edge_index, 1, MPI_INT, solve_options.head(), 10, solve_options.comm(), MPI_STATUS_UNUSED);

		if (edge_index < 0)
			break;

		SampleEdgeFixed(edge_index, V, sampler_options, solve_options, target_num_samples);

		ReportEdgeWorker(edge_index);
	}


}


void Curve::ReportEdgeMaster(V,)
{
	// sam, here

	// receive note from worker *** that edge ___ is done
	// receive finished_edge_index
	int finished_edge_index;
	MPI_Status statty_mc_gatty;

	MPI_Recv(&finished_edge_index,..., MPI_ANY_SOURCE,&statty_mc_gatty);
	int whos_talking = statty_mc_gatty.MPI_SOURCE;

	// get "old" edge sample indices
	ReceiveEdgeSamples(edge_index,head()); // now this->

	SynchronizeVertexSetMaster(edge_index, V,head());

}


void Curve::ReportEdgeWorker(int edge_index, V)
{
	// sam, here

	// send to master that are done BY:
	// send edge_index to master
	MPI_Send(&edge_index, ..., head(), comm());

	SendEdgeSamples(edge_index,head());

	SynchronizeVertexSetWorker(edge_index, V, head());
	// synchronize vertex set and curve
}


void Curve::SynchronizeVertexSetMaster(edge_index, VertexSet & V,int source)
{
	//
	int num_to_recv;
	MPI_Recv(&num_to_recv,...);
	for (num_to_recv)
	{
		// get old_index INDEX

		Vertex v_temp;
		v_temp.receive(source);

		//add to V, getting new index (as return val)
		// int new_index = V.add_vertex(v_temp)

		// adjust the indices in edge_samples 

		// loop over the samples (indices), replace instances of old_index with new_index.
	}
}


void Curve::SynchronizeVertexSetWorker(VertexSet & V,head())
{
	//
	// send num 
	for (num_prev, num_now)
	{
		// 
		send old vertex INDEX
		// send vertex to master
		V[ind].send(head());
	}
	// send done
}





void Curve::FixedSampler(VertexSet & V,
									  sampler_configuration & sampler_options,
									  SolverConfiguration & solve_options,
									  int target_num_samples)
{
	
	if (sampler_options.use_parallel())
		FixedSamplerMaster(V, sampler_options, solve_options, target_num_samples);
	else
		FixedSamplerSerial(V, sampler_options, solve_options, target_num_samples);
}


		//////////////////////////////
		//
		//   Edge sampling methods
		//
		///////////////////////////////


void Curve::SampleEdgeAdaptiveMovement(	int ii,
								VertexSet & V,
								sampler_configuration & sampler_options,
								SolverConfiguration & solve_options)
{
	bool prev_state = solve_options.force_no_parallel();// create a backup value to restore to.
	solve_options.force_no_parallel(true);

	parse_input_file(input_filename()); // restores all the temp files generated by the parser, to this folder.
	solve_options.get_PPD();
	


	WitnessSet W;
	W.set_input_filename(input_filename());
	W.set_num_variables(num_variables());
	W.set_num_natural_variables(V.num_natural_variables());
    W.get_variable_names(num_variables());
	W.copy_patches(*this);
	
	
	
	
	
	this->randomizer()->setup( W.num_variables()-W.num_patches()-1, solve_options.PPD.num_funcs);
	
	
	
	
	
	
	
	
	V.set_curr_projection(pi(0));
	V.set_curr_input(input_filename());
	
	
	
	int	num_vars = this->num_variables();
	
	
	WitnessSet Wnew;
	
	
	vec_mp target_projection;
	init_vec_mp(target_projection,num_vars); target_projection->size = num_vars;
	vec_cp_mp(target_projection,pi(0)); // copy the projection into target_projection
	
	
	vec_mp startpt;
	init_vec_mp(startpt,num_vars); startpt->size = num_vars;
	
	vec_mp estimated_point;
	init_vec_mp(estimated_point,num_vars); estimated_point->size = num_vars-1;
	
	
	vec_mp start_projection;
	init_vec_mp(start_projection,num_vars);  start_projection->size = num_vars;
	vec_cp_mp(start_projection,pi(0)); // grab the projection, copy it into start_projection
	
	
	vec_mp new_point_dehomogenized; init_vec_mp(new_point_dehomogenized, num_vars-1);
	
	comp_mp temp, temp1, target_projection_value;
	init_mp(temp);  init_mp(temp1); init_mp(target_projection_value);
    
	Vertex temp_vertex;


    int prev_num_samp, sample_counter;
	std::vector<bool> refine_current, refine_next;
	mpf_t dist_moved; mpf_init(dist_moved);
	int interval_counter;
	
	std::vector<int> current_indices;
	
	MultilinConfiguration ml_config(solve_options, this->randomizer());


	int num_refinements;
	adaptive_set_initial_refinement_flags(num_refinements,
										  refine_current,
										  current_indices,
										  V,
										  ii, sampler_options);
	
	prev_num_samp = num_samples_on_edge(ii); // grab the number of points from the array of integers
	
	
	
	int pass_number  = 0;//this should be the only place this is reset.
	while (1) // breaking condition is all samples being less than TOL away from each other (in the infty norm sense).
	{
        
		refine_next.resize(prev_num_samp+num_refinements-1); // allocate refinement flag
        
		std::vector<int> new_indices(prev_num_samp+num_refinements);
		
		new_indices[0] = current_indices[0];
		sample_counter = 1; // this will be incremented every time we put a point into new_points
							// starts at 1 because already committed one.
							// this should be the only place this counter is reset.
		
		
		if (sampler_options.verbose_level()>=1)
			printf("edge %d, pass %d, %d refinements\n",ii,pass_number,num_refinements);
		
		if (sampler_options.verbose_level()>=2) {
			printf("the current indices:\n");
			for (int jj=0; jj<prev_num_samp; jj++)
				printf("%d ",current_indices[jj]);
			printf("\n\n");
		}
		
		if (sampler_options.verbose_level()>=3) {
			printf("refine_flag:\n");
			for (int jj=0; jj<prev_num_samp-1; jj++) {
                printf("%s ",refine_current[jj]?"1":"0");
			}
			printf("\n\n");
		}
		
        
		
		num_refinements = 0; // reset this counter.  this should be the only place this is reset
		interval_counter = 0;
		for (int jj=0; jj<prev_num_samp-1; jj++) // for each interval in the previous set
		{
			
			
			
			if (sampler_options.verbose_level()>=2)
				printf("interval %d of %d\n",jj,prev_num_samp-1);
			
			
			
			int startpt_index; int left_index; int right_index;
			
			// set the starting projection and point.
			if (jj==0){// if on the first sample, go the right
				startpt_index = current_indices[1]; //right!
			}
			else{ //go to the left
				startpt_index = current_indices[jj]; // left!
			}
			
			left_index = current_indices[jj];
			right_index = current_indices[jj+1];
			
			
			if (new_indices[sample_counter-1]!=left_index)
				throw std::runtime_error("index mismatch in adaptive edge sampler");

			
			if (refine_current[jj])
			{
                
				vec_cp_mp(startpt,V[startpt_index].point());
				set_mp(&(start_projection->coord[0]), &(V[startpt_index].projection_values())->coord[0]);
				neg_mp(&(start_projection->coord[0]), &(start_projection->coord[0]));
				
				
				estimate_new_projection_value(target_projection_value,				// the new value
											  estimated_point,
                                              V[left_index].point(),	//
                                              V[right_index].point(), // two points input
                                              pi(0));												// projection (in homogeneous coordinates)
				
				
                
				neg_mp(&target_projection->coord[0],target_projection_value); // take the opposite :)
				
				
				set_witness_set_mp(W, start_projection,startpt); // set the witness point and linear in the input for the lintolin solver.
				
                
				if (sampler_options.verbose_level()>=3) {
					print_point_to_screen_matlab(W.point(0),"startpt");
					print_comp_matlab(& W.linear(0)->coord[0],"initial_projection_value");
					print_comp_matlab(target_projection_value,"target_projection_value");
				}
				
				if (sampler_options.verbose_level()>=5)
					W.print_to_screen();
				
				if (sampler_options.verbose_level()>=10) {
					mypause();
				}
				
				
				SolverOutput fillme;
				multilin_solver_master_entry_point(W,         // WitnessSet
                                                   fillme, // the new data is put here!
                                                   &target_projection,
                                                   ml_config,
                                                   solve_options);
				
				fillme.get_noninfinite_w_mult_full(Wnew);
				
				if (Wnew.num_points()==0)
					throw std::runtime_error("tracker did not return any points.");

				
				if (sampler_options.verbose_level()>=3)
					print_point_to_screen_matlab(Wnew.point(0), "new_solution");
				
				
				dehomogenize(&new_point_dehomogenized,Wnew.point(0));
				
				
				// check how far away we were from the LEFT interval point
				norm_of_difference_mindim(dist_moved,
                                   new_point_dehomogenized, // the current new point
                                   estimated_point);// the estimated point which generated the new point
				
				if ( mpf_cmp(dist_moved, sampler_options.TOL )>0  || (pass_number+1 < sampler_options.minimum_num_iterations)){
					refine_next[interval_counter] = true;
					refine_next[interval_counter+1] = true;
					num_refinements+=2;
				}
				else{
                    refine_next[interval_counter] = false;
					refine_next[interval_counter+1] = false;
				}
				
				interval_counter+=2;
				
				
				
				vec_cp_mp(temp_vertex.point(),Wnew.point(0));
				temp_vertex.set_type(Curve_sample_point);
				
                if (sampler_options.no_duplicates){
					new_indices[sample_counter] = index_in_vertices_with_add(V, temp_vertex);
				}
				else{
					new_indices[sample_counter] = V.add_vertex(temp_vertex);
				}
				
				sample_counter++;
				
				new_indices[sample_counter] = right_index;
				sample_counter++;
                
				Wnew.reset();
				
			}
			else {
				if (sampler_options.verbose_level()>=2)
					printf("adding sample %d\n",sample_counter);
				
				refine_next[interval_counter] = 0;
				new_indices[sample_counter] = right_index;
				interval_counter++;
				sample_counter++;
			}
		}
        
		
		if (sampler_options.verbose_level()>=1) // print by default
			printf("\n\n");
		
		if( (num_refinements == 0) || (pass_number >= sampler_options.maximum_num_iterations) ) // if have no need for new samples
		{
			sample_indices_[ii].swap(new_indices);
			break; // BREAKS THE WHILE LOOP
		}
		else{
			
			refine_current = refine_next; // reassign this pointer
			current_indices.swap(new_indices);
            
			prev_num_samp=sample_counter; // update the number of samples
			pass_number++;
		}
        
	}//while loop


	if (sampler_options.verbose_level()>=2)
		printf("done sampling edge %d\n", ii);




	clear_mp(temp); clear_mp(temp1); clear_mp(target_projection_value);
	clear_vec_mp(start_projection); clear_vec_mp(target_projection);
	clear_vec_mp(startpt);
	clear_vec_mp(estimated_point);
    mpf_clear(dist_moved);

    solve_options.force_no_parallel(prev_state);
}






void Curve::SampleEdgeAdaptiveDistance(	int ii,
								VertexSet & V,
								sampler_configuration & sampler_options,
								SolverConfiguration & solve_options)
{

	bool prev_state = solve_options.force_no_parallel();// create a backup value to restore to.
	solve_options.force_no_parallel(true);
	
	parse_input_file(input_filename()); // restores all the temp files generated by the parser, to this folder.
	solve_options.get_PPD();

	WitnessSet W;
	W.set_input_filename(input_filename()); 
	W.set_num_variables(this->num_variables()); 
	W.set_num_natural_variables(V.num_natural_variables());
	W.get_variable_names(num_variables());
	W.copy_patches(*this);
	
	

	this->randomizer()->setup( W.num_variables()-W.num_patches()-1, solve_options.PPD.num_funcs);
	
	
	
	
	V.set_curr_projection(pi(0)); V.set_curr_input(input_filename());
	auto curr_proj_index = V.curr_projection();
	
	
	auto num_vars = num_variables();
	
	
	WitnessSet Wnew;
	
	
	vec_mp target_projection;
	init_vec_mp(target_projection,num_vars); target_projection->size = num_vars;
	vec_cp_mp(target_projection,pi(0)); // copy the projection into target_projection
	
	
	vec_mp startpt;
	init_vec_mp(startpt,num_vars); startpt->size = num_vars;
	
	
	vec_mp start_projection;
	init_vec_mp(start_projection,num_vars);  start_projection->size = num_vars;
	vec_cp_mp(start_projection,pi(0)); // grab the projection, copy it into start_projection
	

	
	
	comp_mp temp, temp1, target_projection_value;
	init_mp(temp);  init_mp(temp1); init_mp(target_projection_value);
	   
	int prev_num_samp, sample_counter;
	std::vector<bool> refine_current, refine_next;
	
	
	

	vec_mp dehom_left, dehom_right;
	init_vec_mp(dehom_left,num_vars-1);   dehom_left->size  = num_vars-1;
	init_vec_mp(dehom_right,num_vars-1);  dehom_right->size = num_vars-1;
	
	
	
	Vertex temp_vertex;
	   
	mpf_t dist_away; mpf_init(dist_away);
	int interval_counter;
	int num_refinements;
	std::vector<int> current_indices;
	
	MultilinConfiguration ml_config(solve_options, this->randomizer());

	adaptive_set_initial_refinement_flags(num_refinements,
                                     refine_current,
                                     current_indices,
                                     V,
                                     ii, sampler_options);
	
	prev_num_samp = num_samples_on_edge(ii); // grab the number of points from the array of integers
	
	
	int pass_number  = 0;//this should be the only place this is reset.
	while((num_refinements>0) && (pass_number < sampler_options.maximum_num_iterations)) // breaking condition is all samples being less than TOL away from each other (in the infty norm sense).
	{
        
		refine_next.resize(prev_num_samp+num_refinements-1); // allocate refinement flag
        
		std::vector<int> new_indices;
		new_indices.resize(prev_num_samp+num_refinements);
		
		new_indices[0] = current_indices[0];
		sample_counter = 1; // this will be incremented every time we put a point into new_points
							// starts at 1 because already committed one.
							// this should be the only place this counter is reset.
		
		if (sampler_options.verbose_level()>=1)
			printf("edge %d, pass %d, %d refinements\n",ii,pass_number,num_refinements);
		
		if (sampler_options.verbose_level()>=2) {
			printf("the current indices:\n");
			for (int jj=0; jj<prev_num_samp; jj++)
				printf("%d ",current_indices[jj]);
			printf("\n\n");
		}
		
		if (sampler_options.verbose_level()>=3) {
			printf("refine_flag:\n");
			for (int jj=0; jj<prev_num_samp-1; jj++) {
                printf("%s ",refine_current[jj]?"1":"0");
			}
			printf("\n\n");
		}
		
        
		
		num_refinements = 0; // reset this counter.  this should be the only place this is reset
		interval_counter = 0;
		for(int jj=0; jj<prev_num_samp-1; jj++) // for each interval in the previous set
		{
			
			
			
			if (sampler_options.verbose_level()>=2)
				printf("interval %d of %d\n",jj,prev_num_samp-1);
			
			
			
			int startpt_index;
			// set the starting projection and point.
			if (jj==0) // if on the first sample, go the right
				startpt_index = current_indices[1]; //right!
			else //go to the left
				startpt_index = current_indices[jj]; // left!
			
			
			auto left_index = current_indices[jj];
			auto right_index = current_indices[jj+1];
			

			if (refine_current[jj])
			{
                
				vec_cp_mp(startpt,V[startpt_index].point());
				set_mp(&(start_projection->coord[0]), &(V[startpt_index].projection_values())->coord[curr_proj_index]);
				neg_mp(&(start_projection->coord[0]), &(start_projection->coord[0]));
				
				
				estimate_new_projection_value(target_projection_value,				// the new value
                                              V[left_index].point(),	//
                                              V[right_index].point(), // two points input
                                              pi(0));												// projection (in homogeneous coordinates)
				
				
                
				neg_mp(&target_projection->coord[0],target_projection_value); // take the opposite :)
				
				
				set_witness_set_mp(W, start_projection, startpt); // set the witness point and linear in the input for the lintolin solver.
				
                
				if (sampler_options.verbose_level()>=3) {
					print_point_to_screen_matlab(W.point(0),"startpt");
					print_comp_matlab(& (W.linear(0))->coord[0],"initial_projection_value");
					print_comp_matlab(target_projection_value,"target_projection_value");
				}
				
				if (sampler_options.verbose_level()>=5)
					W.print_to_screen();
				
				if (sampler_options.verbose_level()>=10) {
					mypause();
				}
				
				
				SolverOutput fillme;
				multilin_solver_master_entry_point(W,         // WitnessSet
                                                   fillme, // the new data is put here!
                                                   &target_projection,
                                                   ml_config,
                                                   solve_options);
				
				fillme.get_noninfinite_w_mult_full(Wnew);
				
				if (Wnew.num_points()==0) {
					std::cout << "tracker did not return any points." << std::endl;
					continue;
				}
				
				if (sampler_options.verbose_level()>=3)
					print_point_to_screen_matlab(Wnew.point(0), "new_solution");
				
				
				
				dehomogenize(&dehom_left,Wnew.point(0),num_vars);
				dehomogenize(&dehom_right,V[left_index].point(),num_vars);
				dehom_right->size = num_vars-1;
				dehom_left->size = num_vars-1;
				
				// check how far away we were from the LEFT interval point
				norm_of_difference_mindim(dist_away,
								   dehom_left, // the current new point
                                   dehom_right);// jj is left, jj+1 is right
				
				if ( mpf_cmp(dist_away, sampler_options.TOL )>0 ){
					refine_next[interval_counter] = true;
					num_refinements++;
				}
				else{
                    refine_next[interval_counter] = false;
				}
				interval_counter++;
				
				
				dehomogenize(&dehom_right,V[right_index].point(),num_vars);
				dehom_right->size = num_vars-1;
				
				// check how far away we were from the RIGHT interval point
				norm_of_difference_mindim(dist_away,
                                   dehom_left, // the current new point
                                   dehom_right);
				
				if (mpf_cmp(dist_away, sampler_options.TOL ) > 0){
					refine_next[interval_counter] = 1;
					num_refinements++;
				}
				else{
					refine_next[interval_counter] = 0;
				}
				interval_counter++;
				
				
				vec_cp_mp(temp_vertex.point(),Wnew.point(0));
				temp_vertex.set_type(Curve_sample_point);
				
                if (sampler_options.no_duplicates){
					new_indices[sample_counter] = index_in_vertices_with_add(V, temp_vertex);
				}
				else{
					new_indices[sample_counter] = V.add_vertex(temp_vertex);
				}
				
				sample_counter++;
				
				new_indices[sample_counter] = right_index;
				sample_counter++;
                
				Wnew.reset();
				
			}
			else {
				if (sampler_options.verbose_level()>=2)
					printf("adding sample %d\n",sample_counter);
				
				refine_next[interval_counter] = false;
				new_indices[sample_counter] = right_index;
				interval_counter++;
				sample_counter++;
			}
		} // re: for jj
        
        if (pass_number<sampler_options.minimum_num_iterations)
		{
			for (int uu = 0; uu < refine_next.size(); ++uu)
				refine_next[uu] = true;
			num_refinements = refine_next.size();
		}

		refine_current = refine_next; // reassign 
		current_indices.swap(new_indices);
        
		prev_num_samp=sample_counter; // update the number of samples
		pass_number++;
        
	}//while loop

	sample_indices_[ii].swap(current_indices);

	if (sampler_options.verbose_level()>=2)
		printf("done distance-sampling edge\n");



	
	clear_mp(temp); clear_mp(temp1); clear_mp(target_projection_value);
	clear_vec_mp(start_projection); clear_vec_mp(target_projection);
	clear_vec_mp(dehom_right); clear_vec_mp(dehom_left);
	clear_vec_mp(startpt);
    mpf_clear(dist_away);

    solve_options.force_no_parallel(prev_state);
}




void Curve::SampleEdgeSemiFixed(	int ii,
								VertexSet & V,
								sampler_configuration & sampler_options,
								SolverConfiguration & solve_options,
								std::vector<int> const& num_samples_per_interval)
{
	bool prev_state = solve_options.force_no_parallel();// create a backup value to restore to.
	solve_options.force_no_parallel(true);

	WitnessSet W;
	
	
	parse_input_file(input_filename()); // restores all the temp files generated by the parser, to this folder.
	
	solve_options.get_PPD();
	
	W.set_input_filename(input_filename());
	W.set_num_variables(num_variables());
	W.set_num_natural_variables(V.num_natural_variables());
    

	W.copy_patches(*this);
	
	
	
	

	this->randomizer()->setup(W.num_variables()-W.num_patches()-1, solve_options.PPD.num_funcs);
	
	
	
	
	

	V.set_curr_projection(pi(0));
	V.set_curr_input(W.input_filename());
	
	

	
	
	
	//where we will move to
	vec_mp target_projection; init_vec_mp2(target_projection,1,1024); target_projection->size = 1;
	vec_cp_mp(target_projection,pi(0)); // copy the projection into target_projection
	
	if (target_projection->size < W.num_variables()) {
		increase_size_vec_mp(target_projection,W.num_variables()); target_projection->size = W.num_variables();
		for (int ii=W.num_natural_variables(); ii<W.num_variables(); ii++) {
			set_zero_mp(&target_projection->coord[ii]);
		}
	}
	
	W.add_linear(target_projection); // grab copy the (possibly increased) projection into start_projection
	

	
	comp_mp temp, temp1, temp2, target_projection_value;
	init_mp2(temp,1024);  init_mp2(temp1,1024); init_mp2(temp2,1024); init_mp2(target_projection_value,1024);
    
	comp_mp proj_interval_width; init_mp(proj_interval_width);
	comp_mp proj_interval_left_endpoint; init_mp(proj_interval_left_endpoint);

	Vertex temp_vertex;
    WitnessSet Wnew; // to hold the output


	MultilinConfiguration ml_config(solve_options,this->randomizer());
	

	comp_mp interval_width; init_mp2(interval_width,1024); set_zero_mp(interval_width);
	comp_mp num_intervals;  init_mp2(num_intervals,1024); set_zero_mp(num_intervals);
	
	vec_mp pre_cycle_scaled_p; init_vec_mp(pre_cycle_scaled_p,0); pre_cycle_scaled_p->size = 0;
	vec_mp final_proj_values; init_vec_mp(final_proj_values,0); final_proj_values->size = 0;


	// set up the starting linear and point
	neg_mp(& (W.linear(0))->coord[0],&(V[get_edge(ii).midpt()].projection_values())->coord[V.curr_projection()]);
	W.reset_points();
	W.add_point(V[get_edge(ii).midpt()].point());




	auto interval_ind = ProjectionIntervalIndex(ii, V);
	auto num_samples_on_edge = num_samples_per_interval[interval_ind]-1;
	mpf_set_d(num_intervals->r,static_cast<double>(num_samples_on_edge));

	set_one_mp(interval_width);
	div_mp(interval_width,interval_width,num_intervals);


	// these are not strictly necessary, but they make the later lines easier to read
	sub_mp(proj_interval_width,&(V[get_edge(ii).right()].projection_values())->coord[V.curr_projection()],&(V[get_edge(ii).left()].projection_values())->coord[V.curr_projection()]);
	set_mp(proj_interval_left_endpoint, &(V[get_edge(ii).left()].projection_values())->coord[V.curr_projection()]);
	
	// delete this line?
	// set_mp(target_projection_value,&(V[get_edge(ii).left()].projection_values())->coord[V.curr_projection()]);
	
	change_size_vec_mp(pre_cycle_scaled_p, num_samples_on_edge);
	pre_cycle_scaled_p->size = num_samples_on_edge;

	change_size_vec_mp(final_proj_values, num_samples_on_edge);
	final_proj_values->size = num_samples_on_edge;
	
	set_zero_mp(&pre_cycle_scaled_p->coord[0]);
	for (int qq=1; qq<num_samples_on_edge; ++qq)
	{
		// compute the raw proj value in [0,1]
		add_mp(&pre_cycle_scaled_p->coord[qq],&pre_cycle_scaled_p->coord[qq-1],interval_width);

		int left_cycle_num, right_cycle_num;
		if (sampler_options.use_uniform_cycle_num)
		{
			left_cycle_num = sampler_options.cycle_num;
			right_cycle_num = sampler_options.cycle_num;
		}
		else
		{
			const auto c_nums = GetMetadata(ii);
			// use the cycle numbers to scale toward the endpoints.
			left_cycle_num = c_nums.CycleNumLeft();
			right_cycle_num = c_nums.CycleNumRight();
		}
		
		
		ScaleByCycleNum(temp2, &pre_cycle_scaled_p->coord[qq], left_cycle_num, right_cycle_num);

		// finally, scale the cycle-number-scaled values into the interval we are working on, 
		// to make the final projection values
		mul_mp(temp1, temp2, proj_interval_width);
		add_mp(&final_proj_values->coord[qq], proj_interval_left_endpoint, temp1);
	}

	

	

	for (int jj=1; jj<num_samples_on_edge; jj++) {
		
		
		
		neg_mp(&target_projection->coord[0],&final_proj_values->coord[jj]); // take the opposite :)
		
		
		
		if (sampler_options.verbose_level()>=3) {
			print_point_to_screen_matlab(W.point(0),"startpt");
			print_comp_matlab(&W.linear(0)->coord[0],"initial_projection_value");
			print_comp_matlab(&target_projection->coord[0],"target_projection_value");
		}
		
		if (sampler_options.verbose_level()>=5)
			W.print_to_screen();
		
		
		SolverOutput fillme;
		multilin_solver_master_entry_point(W,         // WitnessSet
										   fillme, // the new data is put here!
										   &target_projection,
										   ml_config,
										   solve_options);
		
		fillme.get_noninfinite_w_mult_full(Wnew);
		
		if (Wnew.num_points()==0) {
			std::cout << "curve sampler returned no points!" << std::endl;
			break;
			//TODO: ah shit!  this ain't good.  how to deal with it?
		}
		
		vec_cp_mp(temp_vertex.point(),Wnew.point(0));
		temp_vertex.set_type(Curve_sample_point);
		
		if (sampler_options.no_duplicates){
			sample_indices_[ii][jj] = index_in_vertices_with_add(V, temp_vertex);
		}
		else
		{
			sample_indices_[ii][jj] = V.add_vertex(temp_vertex);
		}
		
		
		Wnew.reset();
		add_mp(target_projection_value,target_projection_value,interval_width); // increment this thingy
		
		
	} // re: jj


	
	clear_mp(temp); clear_mp(temp1); clear_mp(temp2);
	clear_vec_mp(target_projection);
	clear_vec_mp(pre_cycle_scaled_p);
	clear_vec_mp(final_proj_values);
	clear_mp(target_projection_value);
	clear_mp(interval_width);
	clear_mp(num_intervals);

	solve_options.force_no_parallel(prev_state);
}




void Curve::SampleEdgeFixed(	int ii,
								VertexSet & V,
								sampler_configuration & sampler_options,
								SolverConfiguration & solve_options,
									  int target_num_samples)
{
	bool prev_state = solve_options.force_no_parallel();// create a backup value to restore to.
	solve_options.force_no_parallel(true);

	WitnessSet W;
	
	
	parse_input_file(input_filename()); // restores all the temp files generated by the parser, to this folder.
	
	solve_options.get_PPD();
	
	W.set_input_filename(input_filename());
	W.set_num_variables(num_variables());
	W.set_num_natural_variables(V.num_natural_variables());
    

	W.copy_patches(*this);
	
	
	
	

	this->randomizer()->setup(W.num_variables()-W.num_patches()-1, solve_options.PPD.num_funcs);
	
	
	
	
	

	
	V.set_curr_projection(pi(0));
	V.set_curr_input(W.input_filename());
	
	

	
	
	
	//where we will move to
	vec_mp target_projection; init_vec_mp2(target_projection,1,1024); target_projection->size = 1;
	vec_cp_mp(target_projection,pi(0)); // copy the projection into target_projection
	
	if (target_projection->size < W.num_variables()) {
		increase_size_vec_mp(target_projection,W.num_variables()); target_projection->size = W.num_variables();
		for (int ii=W.num_natural_variables(); ii<W.num_variables(); ii++) {
			set_zero_mp(&target_projection->coord[ii]);
		}
	}
	
	W.add_linear(target_projection); // grab copy the (possibly increased) projection into start_projection
	

	
	comp_mp temp, temp1, target_projection_value;
	init_mp2(temp,1024);  init_mp2(temp1,1024); init_mp2(target_projection_value,1024);
    
	
	
	Vertex temp_vertex;
    WitnessSet Wnew; // to hold the output


	MultilinConfiguration ml_config(solve_options,this->randomizer());
	

	comp_mp interval_width; init_mp2(interval_width,1024); set_zero_mp(interval_width);
	comp_mp num_intervals;  init_mp2(num_intervals,1024); set_zero_mp(num_intervals);

	neg_mp(& (W.linear(0))->coord[0],&(V[get_edge(ii).midpt()].projection_values())->coord[V.curr_projection()]);
	
	W.reset_points();
	W.add_point(V[get_edge(ii).midpt()].point());
	
	
	mpf_set_d(num_intervals->r,double(target_num_samples-1));
	
	sub_mp(interval_width,&(V[get_edge(ii).right()].projection_values())->coord[V.curr_projection()],&(V[get_edge(ii).left()].projection_values())->coord[V.curr_projection()]);
	
	div_mp(interval_width,interval_width,num_intervals);
	
	set_mp(target_projection_value,&(V[get_edge(ii).left()].projection_values())->coord[V.curr_projection()]);
	
	//add once to get us off 0
	add_mp(target_projection_value,target_projection_value,interval_width);
	
	for (int jj=1; jj<target_num_samples-1; jj++) {
		
		
		
		neg_mp(&target_projection->coord[0],target_projection_value); // take the opposite :)
		
		
		
		if (sampler_options.verbose_level()>=3) {
			print_point_to_screen_matlab(W.point(0),"startpt");
			print_comp_matlab(&W.linear(0)->coord[0],"initial_projection_value");
			print_comp_matlab(target_projection_value,"target_projection_value");
		}
		
		if (sampler_options.verbose_level()>=5)
			W.print_to_screen();
		
		
		SolverOutput fillme;
		multilin_solver_master_entry_point(W,         // WitnessSet
										   fillme, // the new data is put here!
										   &target_projection,
										   ml_config,
										   solve_options);
		
		fillme.get_noninfinite_w_mult_full(Wnew);
		
		if (Wnew.num_points()==0) {
			std::cout << "curve sampler returned no points!" << std::endl;
			//TODO: ah shit!  this ain't good.  how to deal with it?
		}
		
		vec_cp_mp(temp_vertex.point(),Wnew.point(0));
		temp_vertex.set_type(Curve_sample_point);
		
		if (sampler_options.no_duplicates){
			sample_indices_[ii][jj] = index_in_vertices_with_add(V, temp_vertex);
		}
		else
		{
			sample_indices_[ii][jj] = V.add_vertex(temp_vertex);
		}
		
		
		Wnew.reset();
		add_mp(target_projection_value,target_projection_value,interval_width); // increment this thingy
		
		
	} // re: jj
	
	clear_mp(temp); clear_mp(temp1);
	clear_vec_mp(target_projection);
	clear_mp(target_projection_value);
	clear_mp(interval_width);
	clear_mp(num_intervals);

	solve_options.force_no_parallel(prev_state);
}










int  Curve::adaptive_set_initial_sample_data()
{

	sample_indices_.resize(num_edges());
	for (unsigned int ii=0; ii<num_edges(); ii++)
	{
		if (get_edge(ii).is_degenerate())
			sample_indices_[ii].push_back(get_edge(ii).midpt());
		else
		{
			sample_indices_[ii].resize(3);
			
			sample_indices_[ii][0] = get_edge(ii).left();
			sample_indices_[ii][1] = get_edge(ii).midpt();
			sample_indices_[ii][2] = get_edge(ii).right();
		}
	}
	
	return 0;
}


int  Curve::fixed_set_initial_sample_data(int target_num_samples)
{
	
	sample_indices_.resize(num_edges());
	for (unsigned int ii=0; ii<num_edges(); ii++)
	{
		if (get_edge(ii).is_degenerate())
			sample_indices_[ii].push_back(get_edge(ii).midpt());
		else
		{
			sample_indices_[ii].resize(target_num_samples);
			sample_indices_[ii][0] = get_edge(ii).left();
			sample_indices_[ii][target_num_samples-1] = get_edge(ii).right();
		}
	}
	return 0;
}


int  Curve::semi_fixed_set_initial_sample_data(std::vector<int> const& num_samples_per_interval, VertexSet const& V)
{
	
	sample_indices_.resize(num_edges());
	for (unsigned int ii=0; ii<num_edges(); ii++)
	{
		if (get_edge(ii).is_degenerate())
			sample_indices_[ii].push_back(get_edge(ii).midpt());
		else
		{
			const auto & e = get_edge(ii);
			auto interval_ind = ProjectionIntervalIndex(ii, V);
			auto c = num_samples_per_interval[interval_ind];
			sample_indices_[ii].resize(c);
			sample_indices_[ii][0] = e.left();
			sample_indices_[ii][c-1] = e.right();
		}
	}
	return 0;
}


void Curve::adaptive_set_initial_refinement_flags(int & num_refinements, std::vector<bool> & refine_flags, std::vector<int> & current_indices,
                                  VertexSet &V,
                                  int current_edge, sampler_configuration & sampler_options)
{
	
	num_refinements = 0; // reset to 0
	refine_flags.resize(num_samples_on_edge(current_edge)-1);

	current_indices.resize(num_samples_on_edge(current_edge));

	mpf_t dist_away;  mpf_init(dist_away);
	
	vec_mp dehom1, dehom2;  init_vec_mp(dehom1,num_variables()-1); init_vec_mp(dehom2,num_variables()-1);
	dehom1->size = dehom2->size = num_variables()-1;
	
	current_indices[0] = sample_indices_[current_edge][0];
	for (unsigned int ii=0; ii<num_samples_on_edge(current_edge)-1; ii++) {
		if (sample_index(current_edge,ii) != sample_index(current_edge,ii+1))
		{
			refine_flags[ii] = true;
			num_refinements++;
		}
		else
		{
			refine_flags[ii] = false;
		}
		current_indices[ii+1] = sample_index(current_edge,ii+1);
		
		// for (int jj=0; jj<num_variables()-1; jj++) {
		// 	div_mp(&dehom1->coord[jj],
  //                  &(V[sample_index(current_edge,ii)].point())->coord[jj+1],
  //                  &(V[sample_index(current_edge,ii)].point())->coord[0]);
			
		// 	div_mp(&dehom2->coord[jj],
  //                  &(V[sample_index(current_edge,ii+1)].point())->coord[jj+1],
  //                  &(V[sample_index(current_edge,ii+1)].point())->coord[0]);
		// }
		
		// norm_of_difference_mindim(dist_away, dehom1, dehom2); // get the distance between the two adjacent points.
		
		// if ( mpf_cmp(dist_away, sampler_options.TOL)>0 ){
			
		// }
	}
	
	clear_vec_mp(dehom1); clear_vec_mp(dehom2);
	
	mpf_clear(dist_away);
}






void  Curve::output_sampling_data(boost::filesystem::path base_path) const
{

	boost::filesystem::path samplingName = base_path / "samp.curvesamp";
	
	std::cout << "wrote curve sampling to " << samplingName << std::endl;
	FILE *OUT = safe_fopen_write(samplingName);
	// output the number of vertices
	fprintf(OUT,"%u\n\n",num_edges());
	for (unsigned int ii=0; ii<num_edges(); ii++) {
		fprintf(OUT,"%u\n",num_samples_on_edge(ii));
		for (unsigned int jj=0; jj<num_samples_on_edge(ii); jj++) {
			fprintf(OUT,"%d ",sample_index(ii,jj));
		}
		fprintf(OUT,"\n\n");
	}
	
	fclose(OUT);
}





