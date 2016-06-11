# ggxcc
GGX cube map convolver for ioquake3's OpenGL2 renderer

# So what's this for?
The OpenGL2 renderer in ioquake3 uses cube maps for specular reflection, and its internal shaders assume that the mipmaps in each cube map correspond to a linear roughness scale.  However, the renderer uses a simple downsampling method for these mipmaps, causing reflections to look pixelated and too sharp.

This program does offline GGX convolution for a cubemap, which can then be reinserted into the renderer for proper reflections.

# Why isn't this part of OpenGL2?
At the moment, this operation is too slow to be done at level load.  On my machine, a i5-2500k, it takes about 4 seconds to convolve a single 128x128 cubemap, and levels usually contain 8 or more cubemaps to process.  If in the development of this tool the processing becomes much faster, it will be integrated into the OpenGL2 renderer, but not before.
