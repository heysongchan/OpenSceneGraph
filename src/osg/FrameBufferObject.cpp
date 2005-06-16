/* -*-c++-*- OpenSceneGraph - Copyright (C) 1998-2005 Robert Osfield 
 *
 * This library is open source and may be redistributed and/or modified under  
 * the terms of the OpenSceneGraph Public License (OSGPL) version 0.0 or 
 * (at your option) any later version.  The full license is in LICENSE file
 * included with this distribution, and on the openscenegraph.org website.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * OpenSceneGraph Public License for more details.
*/

// initial FBO support written by Marco Jez, June 2005.

#include <osg/FrameBufferObject>
#include <osg/State>
#include <osg/GLExtensions>
#include <osg/Texture1D>
#include <osg/Texture2D>
#include <osg/Texture3D>
#include <osg/TextureCubeMap>
#include <osg/TextureRectangle>
#include <osg/Notify>

using namespace osg;


/**************************************************************************
 * FBOExtensions
 **************************************************************************/
#define LOAD_FBO_EXT(name)    name = (T##name *)getGLExtensionFuncPtr(#name);

FBOExtensions::FBOExtensions(unsigned int contextID)
:    _supported(false)
{
    if (!isGLExtensionSupported(contextID, "GL_EXT_framebuffer_object"))
        return;
    
    LOAD_FBO_EXT(glBindRenderbufferEXT);
    LOAD_FBO_EXT(glGenRenderbuffersEXT);
    LOAD_FBO_EXT(glRenderbufferStorageEXT);
    LOAD_FBO_EXT(glBindFramebufferEXT);
    LOAD_FBO_EXT(glGenFramebuffersEXT);
    LOAD_FBO_EXT(glCheckFramebufferStatusEXT);
    LOAD_FBO_EXT(glFramebufferTexture1DEXT);
    LOAD_FBO_EXT(glFramebufferTexture2DEXT);
    LOAD_FBO_EXT(glFramebufferTexture3DEXT);
    LOAD_FBO_EXT(glFramebufferRenderbufferEXT);
    LOAD_FBO_EXT(glGenerateMipmapEXT);

    _supported = 
        glBindRenderbufferEXT != 0 &&
        glGenRenderbuffersEXT != 0 &&
        glRenderbufferStorageEXT != 0 &&
        glBindFramebufferEXT != 0 &&
        glGenFramebuffersEXT != 0 &&
        glCheckFramebufferStatusEXT != 0 &&
        glFramebufferTexture1DEXT != 0 &&
        glFramebufferTexture2DEXT != 0 &&
        glFramebufferTexture3DEXT != 0 &&
        glFramebufferRenderbufferEXT != 0 &&
        glGenerateMipmapEXT != 0;
}


/**************************************************************************
 * RenderBuffer
 **************************************************************************/

RenderBuffer::RenderBuffer()
:    Object(),
    _internalFormat(GL_DEPTH_COMPONENT24),
    _width(512),
    _height(512)
{
}

RenderBuffer::RenderBuffer(int width, int height, GLenum internalFormat)
:    Object(),
    _internalFormat(internalFormat),
    _width(width),
    _height(height)
{
}

RenderBuffer::RenderBuffer(const RenderBuffer &copy, const CopyOp &copyop)
:    Object(copy, copyop),
    _internalFormat(copy._internalFormat),
    _width(copy._width),
    _height(copy._height)
{
}

GLuint RenderBuffer::getObjectID(unsigned int contextID, const FBOExtensions *ext) const
{
    GLuint &objectID = _objectID[contextID];

    int &dirty = _dirty[contextID];

    if (objectID == 0)
    {
        ext->glGenRenderbuffersEXT(1, &objectID);
        if (objectID == 0) 
            return 0;
        dirty = 1;
    }

    if (dirty)
    {
        // bind and configure
        ext->glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, objectID);
        ext->glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, _internalFormat, _width, _height);
        dirty = 0;
    }

    return objectID;
}

/**************************************************************************
 * FrameBufferAttachement
 **************************************************************************/

#ifndef GL_TEXTURE_CUBE_MAP_POSITIVE_X
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X  0x8515
#endif

struct FrameBufferAttachment::Pimpl
{
    enum TargetType
    {
        RENDERBUFFER,
        TEXTURE1D,
        TEXTURE2D,
        TEXTURE3D,
        TEXTURECUBE,
        TEXTURERECT
    };
    
    TargetType targetType;
    ref_ptr<RenderBuffer> renderbufferTarget;
    ref_ptr<Texture> textureTarget;
    int cubeMapFace;
    int level;
    int zoffset;

    explicit Pimpl(TargetType ttype = RENDERBUFFER, int lev = 0)
    :    targetType(ttype),
        cubeMapFace(0),
        level(lev),
        zoffset(0)
    {
    }

    Pimpl(const Pimpl &copy)
    :    targetType(copy.targetType),
        renderbufferTarget(copy.renderbufferTarget),
        textureTarget(copy.textureTarget),
        cubeMapFace(copy.cubeMapFace),
        level(copy.level),
        zoffset(copy.zoffset)
    {
    }
};

FrameBufferAttachment::FrameBufferAttachment()
{
    _ximpl = new Pimpl;
}

FrameBufferAttachment::FrameBufferAttachment(const FrameBufferAttachment &copy)
{
    _ximpl = new Pimpl(*copy._ximpl);
}

FrameBufferAttachment::FrameBufferAttachment(RenderBuffer* target)
{
    _ximpl = new Pimpl(Pimpl::RENDERBUFFER);
    _ximpl->renderbufferTarget = target;
}

FrameBufferAttachment::FrameBufferAttachment(Texture1D* target, int level)
{
    _ximpl = new Pimpl(Pimpl::TEXTURE1D, level);
    _ximpl->textureTarget = target;
}

FrameBufferAttachment::FrameBufferAttachment(Texture2D* target, int level)
{
    _ximpl = new Pimpl(Pimpl::TEXTURE2D, level);
    _ximpl->textureTarget = target;
}

FrameBufferAttachment::FrameBufferAttachment(Texture3D* target, int level, int zoffset)
{
    _ximpl = new Pimpl(Pimpl::TEXTURE3D, level);
    _ximpl->textureTarget = target;
    _ximpl->zoffset = zoffset;
}

FrameBufferAttachment::FrameBufferAttachment(TextureCubeMap* target, int face, int level)
{
    _ximpl = new Pimpl(Pimpl::TEXTURECUBE, level);
    _ximpl->textureTarget = target;
    _ximpl->cubeMapFace = face;
}

FrameBufferAttachment::FrameBufferAttachment(TextureRectangle* target)
{
    _ximpl = new Pimpl(Pimpl::TEXTURERECT);
    _ximpl->textureTarget = target;
}

FrameBufferAttachment::~FrameBufferAttachment()
{
    delete _ximpl;
}

FrameBufferAttachment &FrameBufferAttachment::operator = (const FrameBufferAttachment &copy)
{
    delete _ximpl;
    _ximpl = new Pimpl(*copy._ximpl);
    return *this;
}

void FrameBufferAttachment::attach(State &state, GLenum attachment_point, const FBOExtensions* ext) const
{
    unsigned int contextID = state.getContextID();

    // force compile texture if necessary
    Texture::TextureObject *tobj = 0;
    if (_ximpl->textureTarget.valid())
    {
        tobj = _ximpl->textureTarget->getTextureObject(contextID);
        if (!tobj || tobj->_id == 0)
        {
            _ximpl->textureTarget->compileGLObjects(state);
            tobj = _ximpl->textureTarget->getTextureObject(contextID);
        }
        if (!tobj || tobj->_id == 0)
            return;
    }

    switch (_ximpl->targetType)
    {
    default:
    case Pimpl::RENDERBUFFER:
        ext->glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, attachment_point, GL_RENDERBUFFER_EXT, _ximpl->renderbufferTarget->getObjectID(contextID, ext));
        break;
    case Pimpl::TEXTURE1D:
        ext->glFramebufferTexture1DEXT(GL_FRAMEBUFFER_EXT, attachment_point, GL_TEXTURE_1D, tobj->_id, _ximpl->level);
        break;
    case Pimpl::TEXTURE2D:
        ext->glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, attachment_point, GL_TEXTURE_2D, tobj->_id, _ximpl->level);
        break;
    case Pimpl::TEXTURE3D:
        ext->glFramebufferTexture3DEXT(GL_FRAMEBUFFER_EXT, attachment_point, GL_TEXTURE_3D, tobj->_id, _ximpl->level, _ximpl->zoffset);
        break;
    case Pimpl::TEXTURERECT:
        ext->glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, attachment_point, GL_TEXTURE_RECTANGLE, tobj->_id, 0);
        break;
    case Pimpl::TEXTURECUBE:
        ext->glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, attachment_point, GL_TEXTURE_CUBE_MAP_POSITIVE_X + _ximpl->cubeMapFace, tobj->_id, _ximpl->level);
        break;
    }
}

int FrameBufferAttachment::compare(const FrameBufferAttachment &fa) const
{
    if (&fa == this) return 0;
    if (_ximpl->targetType < fa._ximpl->targetType) return -1;
    if (_ximpl->targetType > fa._ximpl->targetType) return 1;
    if (_ximpl->renderbufferTarget.get() < fa._ximpl->renderbufferTarget.get()) return -1;
    if (_ximpl->renderbufferTarget.get() > fa._ximpl->renderbufferTarget.get()) return 1;
    if (_ximpl->textureTarget.get() < fa._ximpl->textureTarget.get()) return -1;
    if (_ximpl->textureTarget.get() > fa._ximpl->textureTarget.get()) return 1;
    if (_ximpl->cubeMapFace < fa._ximpl->cubeMapFace) return -1;
    if (_ximpl->cubeMapFace > fa._ximpl->cubeMapFace) return 1;
    if (_ximpl->level < fa._ximpl->level) return -1;
    if (_ximpl->level > fa._ximpl->level) return 1;
    if (_ximpl->zoffset < fa._ximpl->zoffset) return -1;
    if (_ximpl->zoffset > fa._ximpl->zoffset) return 1;
    return 0;
}

/**************************************************************************
 * FrameBufferObject
 **************************************************************************/

FrameBufferObject::FrameBufferObject()
:    StateAttribute()
{
}

FrameBufferObject::FrameBufferObject(const FrameBufferObject &copy, const CopyOp &copyop)
:    StateAttribute(copy, copyop),
    _attachments(copy._attachments)
{
}

void FrameBufferObject::apply(State &state) const
{
    unsigned int contextID = state.getContextID();

    if (_unsupported[contextID])
        return;

    FBOExtensions* ext = FBOExtensions::instance(contextID);
    if (!ext->isSupported())
    {
        _unsupported[contextID] = 1;
        notify(WARN) << "Warning: EXT_framebuffer_object is not supported" << std::endl;
        return;
    }

    if (_attachments.empty())
    {
        ext->glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
        return;
    }

    int &dirtyAttachmentList = _dirtyAttachmentList[contextID];

    GLuint &fboID = _fboID[contextID];
    if (fboID == 0)
    {
        ext->glGenFramebuffersEXT(1, &fboID);
        if (fboID == 0)
        {
            notify(WARN) << "Warning: FrameBufferObject: could not create the FBO" << std::endl;
            return;
        }
        dirtyAttachmentList = 1;
    }

    ext->glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fboID);

    if (dirtyAttachmentList)
    {
        for (AttachmentMap::const_iterator i=_attachments.begin(); i!=_attachments.end(); ++i)
        {
            const FrameBufferAttachment &fa = i->second;
            fa.attach(state, i->first, ext);
        }        
        dirtyAttachmentList = 0;
    }
}

int FrameBufferObject::compare(const StateAttribute &sa) const
{
    COMPARE_StateAttribute_Types(FrameBufferObject, sa);
    COMPARE_StateAttribute_Parameter(_attachments.size());
    AttachmentMap::const_iterator i = _attachments.begin();
    AttachmentMap::const_iterator j = rhs._attachments.begin();
    for (; i!=_attachments.end(); ++i, ++j)
    {
        int cmp = i->second.compare(j->second);
        if (cmp != 0) return cmp;
    }
    return 0;
}
