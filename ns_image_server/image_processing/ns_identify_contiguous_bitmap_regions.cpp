#include "ns_identify_contiguous_bitmap_regions.h"

#include "ns_image_easy_io.h"

using namespace std;
unsigned long ns_connected_component_analyzer::get_new_plabel(){
	unsigned long s = (unsigned long)labels.size();//indicies start at 1 to allow 0 to indicate null 
	labels.resize(s+1);
	eq_classes.resize(s+1);
	return s;
}


void ns_connected_component_analyzer::expand_bitmap_from_point(const ns_vector_2i& pos, ns_image_bitmap& output) {
	for (unsigned int y = 0; y < output.properties().height; y++)
		for (unsigned int x = 0; x < output.properties().width; x++)
			output[y][x] = 0;

	unsigned long label_at_pos = 0;
	for (unsigned int r = 0; r < runs.size(); r++) {
		if (runs[r].row == pos.y && runs[r].start_col <= pos.x && runs[r].end_col >= pos.x) {
			label_at_pos = runs[r].perm_label;
		}
	}
	if (label_at_pos == 0) {
		std::cerr << "label is zero\n";
		return;
	}
	for (unsigned int r = 0; r < runs.size(); r++) {
		if (runs[r].perm_label == label_at_pos) {
			for (unsigned int x = runs[r].start_col; x <= runs[r].end_col; x++) {
				output[runs[r].row][x] = 1;
			}
		}
	}

}

void ns_connected_component_analyzer::generate_objects_from_equivalency_table(const ns_image_properties & default_prop, vector<ns_detected_object *> & output){
	unsigned long number_of_regions(0);
	//allocate objects and find bounding rectangles
	vector<ns_detected_object *> regs(runs.size(),0);
	for (unsigned int r = 0; r < runs.size(); r++){
		unsigned int i = runs[r].perm_label;
		if (i == 0) continue;
		if (regs[i] == 0){
			number_of_regions++;
			regs[i] = new ns_detected_object();
			regs[i]->offset_in_source_image.x = runs[r].start_col;
			regs[i]->size.x = runs[r].end_col-runs[r].start_col+1;

			regs[i]->offset_in_source_image.y= runs[r].row;
			regs[i]->size.y=1;
		}
		else{
			if (runs[r].start_col < regs[i]->offset_in_source_image.x){
				regs[i]->size.x += regs[i]->offset_in_source_image.x - runs[r].start_col;
				regs[i]->offset_in_source_image.x = runs[r].start_col;
			}
			if (runs[r].end_col > regs[i]->offset_in_source_image.x + regs[i]->size.x-1)
				regs[i]->size.x = runs[r].end_col-regs[i]->offset_in_source_image.x+1;

			if (runs[r].row < regs[i]->offset_in_source_image.y){
				regs[i]->size.y += regs[i]->offset_in_source_image.y - runs[r].row;
				regs[i]->offset_in_source_image.y = runs[r].row;
			}
			if (runs[r].row > regs[i]->offset_in_source_image.y + regs[i]->size.y-1)
				regs[i]->size.y = runs[r].row - regs[i]->offset_in_source_image.y + 1;
		}
	}
	//copy over the regions to the output vector and allocate images
	output.resize(number_of_regions);
	unsigned long cur_reg(0);
	for (unsigned long i = 0; i < regs.size(); i++){
		if (regs[i] == 0) continue;
		//copy to output vector
		output[cur_reg] = regs[i];
		cur_reg++;
		//allocate bitmap
		ns_image_properties prop(default_prop);
		prop.width = regs[i]->size.x;
		prop.height= regs[i]->size.y;
		regs[i]->bitmap().init(prop);
		for (unsigned int y = 0; y < prop.height; y++)
			for (unsigned int x = 0; x < prop.width; x++)
				regs[i]->bitmap()[y][x] = 0;
	}
	
	//copy over pixels
	for (unsigned int r = 0; r < runs.size(); r++){
		unsigned int i = runs[r].perm_label;
		if (regs[i] == 0) continue;
		for (unsigned int x = runs[r].start_col; x <= runs[r].end_col; x++){
			regs[i]->bitmap()[runs[r].row - regs[i]->offset_in_source_image.y][x - regs[i]->offset_in_source_image.x] = 1;
		}
		regs[i]->area += runs[r].end_col +1 - runs[r].start_col;
	}


}

void ns_connected_component_analyzer::calculate_equivalency_table(){
	if (runs.size() == 0)
		return;
	//do a downward pass through the data
	for (unsigned long l = 0; l < row_boundaries.size(); l++){
		unsigned long p = row_boundaries[l].start,
					  p_last =  row_boundaries[l].stop;
		unsigned long q,q_last;
		if (l == 0){
			q = 0;
			q_last = 0;
		}
		else{
			q =  row_boundaries[l-1].start;
			q_last =  row_boundaries[l-1].stop;
		}
		if (p == q) continue;

		while(p < p_last && q < q_last){
			if (runs[p].end_col < runs[q].start_col){
				if (runs[q].end_col < runs[p].start_col)q++;
				p++;
			}
			else if (runs[q].end_col < runs[p].start_col)
				q++;
			else{
				if (!(runs[p].end_col < runs[q].start_col || runs[q].end_col < runs[p].start_col)){
					//there is some overlap
					if(runs[p].perm_label == 0)
						runs[p].perm_label = runs[q].perm_label;
					else if (runs[p].perm_label != runs[q].perm_label)
						make_equivalent(runs[p].perm_label,runs[q].perm_label);
				}
				if (runs[p].end_col > runs[q].end_col) q++;
				else if (runs[p].end_col < runs[q].end_col) p++;
				else if (runs[p].end_col == runs[q].end_col){q++; p++;}
			}
		}
		for (p = row_boundaries[l].start; p < p_last; p++){
			if (runs[p].perm_label == 0)
				runs[p].perm_label = get_new_plabel();
			else if (labels[runs[p].perm_label].label !=0)
				runs[p].perm_label = labels[runs[p].perm_label].label;
		}
	}
	//do an upward pass through the data
	for (long l = (long)row_boundaries.size()-1; l > 0; l--){
		unsigned long p = row_boundaries[l].start,
					  p_last =  row_boundaries[l].stop;
		unsigned long q,q_last;
		if (l == (long)row_boundaries.size()-1){
			q = 0;
			q_last = 0;
		}
		else{
			q =  row_boundaries[l+1].start;
			q_last =  row_boundaries[l+1].stop;
		}
		if (p == q) continue;
		while(p < p_last && q < q_last){
			if (runs[p].end_col < runs[q].start_col){
				if (runs[q].end_col < runs[p].start_col) q++;
				p++;
			}
			else if (runs[q].end_col < runs[p].start_col)
				q++;
			else{
				if (!(runs[p].end_col < runs[q].start_col || runs[q].end_col < runs[p].start_col)){
					//there is some overlap
					if (runs[p].perm_label != runs[q].perm_label){
						labels[runs[p].perm_label].label = runs[q].perm_label;
						runs[p].perm_label = runs[q].perm_label;
					}
				}
				if (runs[p].end_col > runs[q].end_col) q++;
				else if (runs[p].end_col < runs[q].end_col) p++;
				else if (runs[p].end_col == runs[q].end_col){q++; p++;}
			}
		}
		for (p = row_boundaries[l].start; p < p_last; p++){
			if (labels[runs[p].perm_label].label != 0)
				runs[p].perm_label = labels[runs[p].perm_label].label;
		}
	}
}


void ns_connected_component_analyzer::make_equivalent(const unsigned long l1,const unsigned long l2){
	if (labels[l1].label == 0 && labels[l2].label==0){
		labels[l1].label = l1;
		labels[l2].label = l1;
		labels[l1].next = l2;
		labels[l2].next = 0;
		if (eq_classes.size() <= l1)
			eq_classes.resize(l1+1,l1);
			eq_classes[l1] = l1;
	}
	else if (labels[l1].label == labels[l2].label)
		return;
	else if (labels[l1].label != 0 && labels[l2].label == 0){
		unsigned long b = labels[l1].label;
		labels[l2].label = b;
		labels[l2].next = eq_classes[b];
		if (eq_classes.size() <= b)
			eq_classes.resize(b+1,l2);
		else eq_classes[b] = l2;
	}
	else if (labels[l1].label == 0 && labels[l2].label != 0){
		unsigned long b = labels[l2].label;
		labels[l1].label = b;
		labels[l1].next = eq_classes[b];
		if (eq_classes.size() <= b)
			eq_classes.resize(b+1,l1);
		else eq_classes[b] = l1;
	}
	else{
		unsigned long b = labels[l2].label;
		unsigned long m = eq_classes[b];
		unsigned long e = labels[l1].label;
		while(labels[m].next != 0){
			labels[m].label = e;
			m = labels[m].next;
		}
		labels[m].label = e;
		labels[m].next = eq_classes[e];
		eq_classes[e] = eq_classes[b];
		eq_classes[b] = 0;
	}
}

