if CLIENT_INCLUDED
noinst_LTLIBRARIES = libGLKit.la
endif

AM_CPPFLAGS = \
	$(SDL_CFLAGS) \
	$(CONF_CPPFLAGS) \
	-I$(top_srcdir)/include
AM_CFLAGS = $(CONF_CFLAGS)
AM_CXXFLAGS = $(CONF_CXXFLAGS)

MAINTAINERCLEANFILES = \
	Makefile.in

libGLKit_la_LDFLAGS = -static
libGLKit_la_SOURCES =			\
	GLBatch.cxx			\
	bzfgl-batch.cxx			\
	OpenGLFramebuffer.cxx		\
	OpenGLGState.cxx		\
	OpenGLLight.cxx			\
	ShadowMapFBO.cxx		\
	ShadowMapper.cxx		\
	TankLightingShader.cxx		\
	OpenGLMaterial.cxx		\
	OpenGLTexture.cxx		\
	OpenGLUtils.cxx		\
	RenderNode.cxx

EXTRA_DIST = \
	README
