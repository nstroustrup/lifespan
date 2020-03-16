#pragma once
#include "ns_image.h"
struct ns_gl_tile {
	enum { tile_x = 256, tile_y = 256 };
	std::vector<ns_8_bit> buffer;
	unsigned int texture;
	void upload();
	
	ns_gl_tile() :buffer(4 * tile_x * tile_y), texture(0), texture_altered_since_last_upload(false){}//rgba
	~ns_gl_tile();
	bool texture_altered_since_last_upload;
};
//rbga buffer
class ns_tiled_gl_image {
public:
	ns_8_bit* operator()(const long& x, const long& y) {
		int t_x = x / ns_gl_tile::tile_x, l_x = x % ns_gl_tile::tile_x,
			t_y = y / ns_gl_tile::tile_y, l_y = y % ns_gl_tile::tile_y;
		buffers[t_y * grid_x + t_x].texture_altered_since_last_upload = true;
		return &buffers[t_y * grid_x + t_x].buffer[4 * (l_y * ns_gl_tile::tile_x + l_x)]; //rgba
	}
	const ns_8_bit* operator()(const long& x, const long& y) const {
		int t_x = x / ns_gl_tile::tile_x, l_x = x % ns_gl_tile::tile_x,
			t_y = y / ns_gl_tile::tile_y, l_y = y % ns_gl_tile::tile_y;
		return &buffers[t_y * grid_x + t_x].buffer[4 * (l_y * ns_gl_tile::tile_x + l_x)]; //rgba
	}
	void resize(const ns_image_properties& p) {
		if (p == prop)
			return;
		grid_x = ceil(p.width / (float)ns_gl_tile::tile_x);
		grid_y = ceil(p.height / (float)ns_gl_tile::tile_y);
		buffers.resize(grid_x * grid_y);
		for (unsigned int i = 0; i < buffers.size(); i++)
			buffers[i].texture_altered_since_last_upload = true;
		prop = p;
	}
	void upload_textures(){
		for (unsigned int i = 0; i < buffers.size(); i++) {
			if (buffers[i].texture_altered_since_last_upload)
				buffers[i].upload();
		}
	}
	void draw(long image_w, long image_h) const;

	const ns_image_properties& properties() const { return prop; }
private:

	std::vector<ns_gl_tile> buffers;	//tiles
	int grid_x, grid_y;
	ns_image_properties prop;
};
