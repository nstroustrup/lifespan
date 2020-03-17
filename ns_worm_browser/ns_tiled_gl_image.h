#pragma once
#include "ns_image.h"
struct ns_gl_tile {
	enum { tile_x = 256, tile_y = 256, border = 4 };

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
	//leave a 1 pixel border around each tile to allow seamless interpolation under linear interpolation rescaling
	static long tile_x() {return ns_gl_tile::tile_x - 2* ns_gl_tile::border; }
	static long tile_y() { return ns_gl_tile::tile_y - 2 * ns_gl_tile::border; }
	ns_8_bit* operator()(const long& x, const long& y) {
		int t_x = x / tile_x(), l_x = x % tile_x(),
			t_y = y / tile_y(), l_y = y % tile_y();
		buffers[t_y * grid_x + t_x].texture_altered_since_last_upload = true;
		return &buffers[t_y * grid_x + t_x].buffer[4 * ((l_y+ ns_gl_tile::border) * ns_gl_tile::tile_x + l_x+ ns_gl_tile::border)]; //rgba
	}
	const ns_8_bit* operator()(const long& x, const long& y) const {
		int t_x = x / tile_x(), l_x = x % tile_y(),
			t_y = y / tile_x(), l_y = y % tile_y();
		return &buffers[t_y * grid_x + t_x].buffer[4 * ((l_y + ns_gl_tile::border) * ns_gl_tile::tile_x + l_x + ns_gl_tile::border)]; //rgba
	}
	void resize(const ns_image_properties& p) {
		if (p == prop)
			return;
		grid_x = ceil(p.width / (float)tile_y());
		grid_y = ceil(p.height / (float)tile_y());
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
