/*
 * Copyright (C) 2006-2025 by the Widelands Development Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef WL_GRAPHIC_IMAGE_CACHE_H
#define WL_GRAPHIC_IMAGE_CACHE_H

#include <map>
#include <memory>

#include "base/macros.h"
#include "graphic/image.h"
#include "graphic/texture.h"

// For historic reasons, most part of the Widelands code base expect that an
// Image stays valid for the whole duration of the program run. This class is
// the one that keeps ownership of all Images to ensure that this is true.
// Other parts of Widelands will create images when they do not exist in the
// cache yet and then put it into the cache and therefore releasing their
// ownership.
class ImageCache {
public:
	using MipMapScale = std::pair<float, const char*>;
	static constexpr unsigned kScalesCount = 4;
	static constexpr MipMapScale kScales[kScalesCount] = {
	   {0.5f, "_0.5"}, {1.f, "_1"}, {2.f, "_2"}, {4.f, "_4"}};
	static constexpr int kNoScale = 0xff;
	static constexpr int kDefaultScaleIndex = 1;

	ImageCache() = default;
	~ImageCache() = default;

	// Insert the 'image' into the cache and returns a pointer to the inserted
	// image for convenience.
	const Image* insert(const std::string& hash, std::unique_ptr<const Image> image);

	// Returns the image associated with the 'hash'. If no image by this hash is
	// known, it will try to load one from disk with the filename = hash. If
	// this fails, it will throw an error.
	// If a scale is provided, the cache will return the image for the mipmap scale
	// at the requested size, or nullptr if the image has no mipmap at this scale.
	const Image* get(std::string hash, bool theme_lookup = true, uint8_t scale_index = kNoScale);

	// Returns true if the 'hash' is stored in the cache.
	[[nodiscard]] bool has(const std::string& hash) const;

	// Fills the image cache with the hash -> Texture map 'textures_in_atlas'
	// and take ownership of 'texture_atlases' so that the textures stay valid.
	void
	fill_with_texture_atlases(std::vector<std::unique_ptr<Texture>> texture_atlases,
	                          std::map<std::string, std::unique_ptr<Texture>> textures_in_atlas);

	uint8_t get_mipmap_bitset(const std::string& hash);

private:
	std::vector<std::unique_ptr<Texture>> texture_atlases_;
	std::map<std::string, std::unique_ptr<const Image>> images_;
	std::map<std::string, uint8_t /* scales bitset */> mipmap_cache_;

	std::vector<std::unique_ptr<const Image>> outdated_images_;
	std::vector<std::unique_ptr<Texture>> outdated_texture_atlases_;

	DISALLOW_COPY_AND_ASSIGN(ImageCache);
};

extern ImageCache* g_image_cache;

#endif  // end of include guard: WL_GRAPHIC_IMAGE_CACHE_H
