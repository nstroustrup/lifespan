#include "ns_spine_drawer.h"
using namespace std;
#define svg_dx 0.1
void ns_spine_drawer::draw_mesh(const vector<ns_triangle_d> & mesh, const ns_color_8 & color, const unsigned int resize_factor,ns_image_standard & output){
	//draw triangles
	for (unsigned int i = 0; i < mesh.size(); i++){
		ns_triangle_i val;
		for (unsigned int t = 0; t < 3; t++){
			val.vertex[t].x = (int)(resize_factor*mesh[i].vertex[t].x);
			val.vertex[t].y = (int)(resize_factor*mesh[i].vertex[t].y);
		}
		output.draw_line_color(val.vertex[0],val.vertex[1],color);
		output.draw_line_color(val.vertex[1],val.vertex[2],color);
		output.draw_line_color(val.vertex[2],val.vertex[0],color);
	}
}

void ns_spine_drawer::draw_normals(const ns_worm_shape & worm, ns_image_standard & output, const unsigned int resize_factor){
	
	const unsigned int o(ns_worm_detection_constants::get(ns_worm_detection_constant::spine_visualization_output_resolution,output.properties().resolution));
	for (unsigned int j = 0; j < worm.nodes.size(); j+=o){
		ns_vector_2i nv[2] = { ns_vector_2i( (int)(worm.normal_0[j].x*resize_factor),
											 (int)(worm.normal_0[j].y*resize_factor)),
							   ns_vector_2i( (int)(worm.normal_1[j].x*resize_factor),
											 (int)(worm.normal_1[j].y*resize_factor))
							 };
		if (nv[0] == ns_vector_2i(0,0) || nv[1] == ns_vector_2i(0,0))
			continue;
		ns_vector_2i c((int)(worm.nodes[j].x*resize_factor),
					   (int)(worm.nodes[j].y*resize_factor));
		output.draw_line_color(c,nv[0]+c, ns_color_8(250,250,250));
		output.draw_line_color(c,nv[1]+c, ns_color_8(250,250,250));
	}
}

void ns_spine_drawer::draw_normals(const ns_worm_shape & worm, ns_svg & svg){
	
	const unsigned int o(ns_worm_detection_constants::get(ns_worm_detection_constant::spine_visualization_output_resolution,3200));
	ns_color_8 colors[2]={ns_color_8(250,250,250),ns_color_8(200,200,200)};
	svg.start_group();
	for (unsigned int i = 0; i < worm.nodes.size(); i++){
		ns_vector_2d nv[2] = { ns_vector_2d( (int)(worm.normal_0[i].x),
											 (int)(worm.normal_0[i].y)),
							   ns_vector_2d( (int)(worm.normal_1[i].x),
											 (int)(worm.normal_1[i].y))
							 };
		if (nv[0] == ns_vector_2d(0,0) || nv[1] == ns_vector_2d(0,0))
			continue;
		ns_vector_2d c(worm.nodes[i].x,worm.nodes[i].y);
		svg.draw_line(c*svg_dx,nv[0]*svg_dx+c*svg_dx, colors[i%2]);
		svg.draw_line(c*svg_dx,nv[1]*svg_dx+c*svg_dx, colors[i%2]);
	}
	svg.end_group();
}
void ns_spine_drawer::draw_edges(ns_svg & svg,const vector<ns_vector_2d> & edge_coordinates){
	vector<ns_vector_2d> points;
	points.insert(points.begin(),edge_coordinates.begin(),edge_coordinates.end());
	svg.start_group();
	for (unsigned int i = 0; i < points.size(); i+=2)
		svg.draw_line(points[i]*svg_dx,points[i+1]*svg_dx,ns_color_8(128,128,200));
	svg.end_group();
	/*if (edge_coordinates.size() != 0)
		points.push_back(edge_coordinates[0]);
	svg.draw_poly_line(points,ns_color_8(128,128,200));*/
}

void ns_spine_drawer::draw_spine(const ns_image_standard & img, const ns_segment_cluster & seg, const ns_worm_shape & worm, ns_svg & svg){
	
	//draw grayscale
	svg.draw_rectangle(ns_vector_2d(0,0),ns_vector_2d(img.properties().width,img.properties().height)*svg_dx,ns_color_8(0,0,0),ns_color_8(0,0,0),1,false);
	svg.start_group();
	for (unsigned int y = 0; y < img.properties().height; y++)
		for (unsigned int x = 0; x < img.properties().width; x++)
			if (img[y][x]!=0)
			svg.draw_rectangle(ns_vector_2d(svg_dx*x,svg_dx*y),ns_vector_2d(svg_dx*(x+1),svg_dx*(y+1)),ns_color_8(img[y][x],img[y][x],img[y][x]),
																					   ns_color_8(img[y][x],img[y][x],img[y][x]),1,false);

	svg.end_group();
	ns_vector_2d vertex[2];
	for(unsigned int i = 0; i < seg.segments.size(); i++){
		if (seg.segments.size() == 0)continue;
		vector<ns_vector_2d> points(seg.segments[i]->nodes.size());
		for (unsigned int j = 0; j < seg.segments[i]->nodes.size(); j++)
			points[j] = seg.segments[i]->nodes[j].position*svg_dx;
		svg.draw_poly_line(points,ns_rainbow<ns_color_8>(((float)i+1)/(float)(seg.segments.size()+1))*.8);
	}

	unsigned int end_d(ns_worm_detection_constants::get(ns_worm_detection_constant::worm_end_node_margin,3200));

	//draw spines
	ns_color_8 end_color_offset(30,10,10);
	ns_color_8 shadow_offset(30,30,30);
	ns_vector_2i offset(1,0);

	ns_color_8 color = ns_rainbow<ns_color_8>(0,(float).05),
			   shadow = ns_color_8::safe_subtraction(color,shadow_offset);
	
	if (worm.nodes.size() != 0){
		vector<ns_vector_2d> points(worm.nodes.size());
		for (unsigned int i = 0; i < worm.nodes.size(); i++)
			points[i] = worm.nodes[i]*svg_dx;
		svg.draw_poly_line(points,color);
	}
}

void ns_spine_drawer::draw_spine(const ns_image_standard & img, const ns_segment_cluster & seg, const ns_worm_shape & worm, ns_image_standard & output, const unsigned int resize_factor){
	
	ns_image_standard im2;
	im2.init(img.properties());
	for (unsigned int y = 0; y < im2.properties().height; y++){
		for (unsigned int x = 0; x < im2.properties().components*im2.properties().width; x++){
			im2[y][x] = img[y][x];
		}
	}

	ns_image_properties outprop = im2.properties();
	outprop.height*=resize_factor;
	outprop.width*=resize_factor;
	outprop.resolution*=resize_factor;
	outprop.components = 3;
	
	//cerr << "Drawing spine RF:" << resize_factor << "(" << outprop.width << "," << outprop.height << ")\n";
	output.init(outprop);

	//enlarge bitmap.
	if (im2.properties().components == 1){
		//b&w image
		for (unsigned int y = 0; y < im2.properties().height; y++){
			for (unsigned int x = 0; x < im2.properties().width; x++)
				for (unsigned int r_y = 0; r_y < resize_factor; r_y++)
					for (unsigned int r_x = 0; r_x < resize_factor; r_x++){
						for (unsigned int c = 0; c < 3; c++)
							output[resize_factor*y + r_y][3*(resize_factor*x+r_x)+c] = im2[y][x];
						#ifdef NS_DRAW_BITMAP_EDGES
						if (edge_bitmap[y][x]!=0){
								output[resize_factor*y + r_y][3*(resize_factor*x+r_x)  ] = 255;
								output[resize_factor*y + r_y][3*(resize_factor*x+r_x)+1] = 0;
								output[resize_factor*y + r_y][3*(resize_factor*x+r_x)+2] = 255;
						}
						#endif
					}
		}
	}
	else{
		//color image
		for (unsigned int y = 0; y < im2.properties().height; y++){
			for (unsigned int x = 0; x < im2.properties().width; x++)
				for (unsigned int r_y = 0; r_y < resize_factor; r_y++)
					for (unsigned int r_x = 0; r_x < resize_factor; r_x++)
						for (unsigned int c = 0; c < 3; c++)
							output[resize_factor*y + r_y][3*(resize_factor*x+r_x)+c] = im2[y][3*x+c];
		}


	}


	#ifdef NS_DRAW_SPINE_NORMALS
	ns_color_8 dk_gray(60,70,80);
	#else
	ns_color_8 dk_gray(170,170,255);
	#endif
	ns_color_8 red(200,15,0);
	ns_color_8 yellow(255,255,30);
	ns_color_8 pink(200,0,200);

	//draw triangles
	//draw_mesh(mesh,dk_gray,resize_factor,output);

	ns_vector_2i vertex[2];
	for(unsigned int i = 0; i < seg.segments.size(); i++){
		if (seg.segments.size() == 0)continue;
		unsigned int j;
		vertex[0].x = (int)(seg.segments[i]->nodes[0].position.x * resize_factor);
		vertex[0].y = (int)(seg.segments[i]->nodes[0].position.y * resize_factor);
		for (j = 1; j < seg.segments[i]->nodes.size(); j++){
			vertex[1].x = (int)(seg.segments[i]->nodes[j].position.x * resize_factor);
			vertex[1].y = (int)(seg.segments[i]->nodes[j].position.y * resize_factor);
			output.draw_line_color(vertex[0],vertex[1], ns_rainbow<ns_color_8>(((float)i+1)/(float)(seg.segments.size()+1))*.8);
			vertex[0] = vertex[1];
		}
	}
	unsigned int end_d(ns_worm_detection_constants::get(ns_worm_detection_constant::worm_end_node_margin,im2.properties().resolution));

	//draw spines
	ns_color_8 end_color_offset(30,10,10);
	ns_color_8 shadow_offset(30,30,30);
	ns_vector_2i offset(1,0);

	ns_color_8 color = ns_rainbow<ns_color_8>(0,(float).05),
			   shadow = ns_color_8::safe_subtraction(color,shadow_offset);
	
	if (worm.nodes.size() != 0){
		vertex[0].x = (int)(worm.nodes[0].x * resize_factor);
		vertex[0].y = (int)(worm.nodes[0].y * resize_factor);
		for (unsigned int i = 0; i < (unsigned int )worm.nodes.size(); i++){
			vertex[1].x = (int)(worm.nodes[i].x * resize_factor);
			vertex[1].y = (int)(worm.nodes[i].y * resize_factor);
			if (i == 1 || i == worm.nodes.size() -1)
				output.draw_line_color(vertex[0],vertex[1], ns_color_8::safe_subtraction(color,end_color_offset),1); //draw endpoints a different color to allow loop disambiguation
			else 
				output.draw_line_color(vertex[0], vertex[1],color,1);
			vertex[0] = vertex[1];	
		}
	}
}

