library(rgl);
ns_plot_3d_animal_movement <- function(worm_data,title_text,plot_movement=1,data_types_to_display=3,color_groups=0,specific_worm=-1,zoom_on_specific=F){
	open3d();

#if we want to display colors based on a point's group id rather than it's path id, make it so
	if(color_groups == 1){
		wd <- worm_data[order(worm_data$hand_annotation,worm_data$path_group_id,worm_data$t,worm_data$slowly_moving,worm_data$low_temporal_resolution,worm_data$movement),]
		
		wd <- wd[sort.list(wd$path_group_id),];
		path_id <- wd$path_group_id;
	}
	else{
		wd <- worm_data[order(worm_data$hand_annotation,worm_data$path_group_id,worm_data$t,worm_data$slowly_moving,worm_data$low_temporal_resolution,worm_data$movement),]
		wd <- wd[sort.list(wd$path_id),];
		path_id <- wd$path_group_id;
	}

#set up the graph axis based on the limits of the data provided
	x_max = max(wd$x);
	y_max = max(wd$y);
	x_min = min(wd$x);
	y_min = min(wd$y);
	t_min = min(wd$t)
	t_max = max(wd$t)
	
	if (zoom_on_specific && specific_worm != -1){
		wds = subset(wd,wd$path_group_id == specific_worm);
		x_max = max(wds$x)*1.1;
		y_max = max(wds$y)*1.1;
		x_min = min(wds$x)*.9;
		y_min = min(wds$y)*.9;
		t_min = min(wds$t)*.9;
		t_max = max(wds$t)*1.1;
	}
	
	pp = plot3d(
	c(),c(),c(),
	xlim=c(t_min,t_max),
	ylim=c(x_min,x_max),
	zlim=c(y_min,y_max),
	xlab="Time (days)",ylab="Position (X)",zlab="Position (Y)",main=title_text,type = "p")

	show_paths <- data_types_to_display==0 || data_types_to_display==1 || data_types_to_display==2 || data_types_to_display==3
	show_fast <- data_types_to_display==1 || data_types_to_display==3 || data_types_to_display==4 || data_types_to_display==5	
	show_slow <- data_types_to_display==2 || data_types_to_display==3 || data_types_to_display==4 || data_types_to_display==6
	show_temp <- show_slow;
	show_all <- data_types_to_display==7
	
	
#set up the rainbow colors to differentiate different paths or groups
	max_path_id = max(path_id);
	if (max_path_id < 10)
		max_path_id = 40;
	if (max_path_id != 0){
		fh <-hsv(h=c(1:max_path_id)/max_path_id*.85+.05,s=1,v=1);	
		sh <-hsv(h=c(1:max_path_id)/max_path_id*.85+.1,s=1,v=.6);
		path_colors <- sample( c(fh,sh));
	}

	
	line_start_pos <- 1
	point_start_pos <- 1
	i <- 1
	slowly_moving_value <- wd$slowly_moving[i]
	low_density <- wd$low_temporal_resolution[i]
	by_hand <- wd$hand_annotation[i]
	pid= path_id[i]
	movement = wd$movement[i]
	last_color = -1;
	last_line_color = -1;
	while(i <= length(path_id)){
		line_changed = path_id[i] != pid || i == length(path_id);
		point_changed = wd$slowly_moving[i] != slowly_moving_value || 
		     wd$low_temporal_resolution[i] != low_density || i == length(path_id) ||
		     wd$hand_annotation[i] != by_hand || wd$movement[i] != movement || path_id[i] != pid;
		     
		if ( line_changed || point_changed){
		     	
			if (by_hand == 0){
				point_type = 20
				
				if (show_all)
					 point_color <- "#000000FF"
					 else
				if (show_fast==1 && pid <= 0) {
				
				    if (low_density==0){
					point_color <- "#000000FF"
					line_color <- -1
					point_size <- 2
				    }
				    else{
					point_color <- "#FF6666FF"
					line_color <- -1
					point_size <- 5
				    }
				}
				else {
				    if (show_paths && pid>0 && slowly_moving_value==0 && (specific_worm==-1 || pid == specific_worm)){
					if (low_density==1){
						point_color <- "#CCFFCCFF"
						point_size <- 5
					}
					else {
						point_color <- "#666666FF"
						point_size <- 5
					}
					line_color <- path_colors[pid]
				    }
				    else {
					point_color <- -1
					point_size <- 1
					line_color <- -1
				    }
				}

			}
			else{
				line_color = -1;
				
				point_type = 21
				point_size <- 1;
				if (movement == 0){
					point_color = "#000000FF"
					point_size = 6
				}
				else if (movement == 1)
					point_color <- "#AAAA00FF"
				else if (movement == 2)
					point_color <- "#00FF00FF"
				else if (movement == 3)
					point_color <- "#FF00FFFF"
				else if (movement == 4)
					point_color <- "#CCCCCCFF"
				else point_color <- "#000000FF";
			}
			#if(pid == specific_worm)
			#	warning(paste(point_color," ",point_changed))
			if (point_changed ){
				if (point_color != -1){
				    if(last_color != point_color || last_line_color!=-1){
					rgl.material(color=point_color,size=point_size,lit=FALSE);
					last_color = point_color;
				    }
				    points3d(wd$t[point_start_pos:(i-1)],wd$x[point_start_pos:(i-1)],y_max-wd$y[point_start_pos:(i-1)]+y_min,size=point_size)	
				}
				point_start_pos = i;
			}
			if (line_changed){
				if (show_paths && pid > 0 && line_color != -1){
				     rgl.material(color=line_color,size=3,lit=FALSE);
				     last_line_color <-line_color
				     lines3d(wd$t[line_start_pos:(i-1)],wd$x[line_start_pos:(i-1)],y_max-wd$y[line_start_pos:(i-1)]+y_min,lwd=2)	

				}
				line_start_pos = i;
			}
			
			slowly_moving_value <- wd$slowly_moving[i]
			value <- path_id[i]
			low_density <- wd$low_temporal_resolution[i]
			by_hand = wd$hand_annotation[i]
			movement = wd$movement[i]
			pid = path_id[i]

		}
		i <- i + 1
	}
#	box3d()
#	axes3d(c('x--'),tick=TRUE,nticks=5)
#	axes3d(c('z--'),tick=TRUE,nticks=5)
#	axes3d(c('z++'),tick=TRUE,nticks=5) 
	rgl.bringtotop()
	material3d(material3d());
	return(wd)
}