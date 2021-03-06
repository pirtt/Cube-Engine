#include "Thumbnail.h"
#include "Rendering/FrameBuffer.h"
#include "Rendering/Renderer.h"
#include "BackEnd/RenderBackEnd.h"
#define GLEW_STATIC
#include <GL/glew.h>
namespace tzw {
	ThumbNail::ThumbNail(Drawable3D* node):m_node(node),m_isDone(false)
	{
		initTexture();
	}

	void ThumbNail::doSnapShot()
	{
		glDisable(GL_SCISSOR_TEST);
		glEnable(GL_DEPTH_TEST);
		glDisable(GL_ALPHA_TEST);
		RenderBackEnd::shared()->setDepthMaskWriteEnable(true);
		m_frameBuffer->bindForWriting();
		m_node->setPos(vec3(0, 0, 0));
		RenderBackEnd::shared()->setClearColor(0.5, 0.5, 0.5);
		RenderBackEnd::shared()->clear();

		auto m = Matrix44();
		m.setToIdentity();
		auto p = Matrix44();
		p.perspective(45, 1, 0.1, 50);
		auto node = Camera();
		node.setPos(0.5, 0.5, 0.5);
		node.lookAt(vec3(0, 0, 0), vec3(0, 1, 0));

		for (int i = 0; i < m_node->getMeshCount(); i++)
		{
	        RenderCommand command(m_node->getMesh(i), m_node->getMaterial(), RenderCommand::RenderType::Common);

    		m_node->setUpCommand(command);
	        m_node->setUpTransFormation(command.m_transInfo);

			command.m_transInfo.m_projectMatrix = p;
			command.m_transInfo.m_viewMatrix = node.getViewMatrix();
			Renderer::shared()->renderCommon(command);
			RenderBackEnd::shared()->setClearColor(0, 0, 0);
		}
	}

	Drawable3D* ThumbNail::getNode() const
	{
		return m_node;
	}

	void ThumbNail::setNode(Drawable3D* const node)
	{
		m_node = node;
	}

	bool ThumbNail::isIsDone() const
	{
		return m_isDone;
	}

	void ThumbNail::setIsDone(const bool isDone)
	{
		m_isDone = isDone;
	}

	Texture* ThumbNail::getTexture() const
	{
		return m_texture;
	}

	void ThumbNail::setTexture(Texture* const texture)
	{
		m_texture = texture;
	}

	void ThumbNail::initTexture()
	{
		m_texture = new Texture();
		m_frameBuffer = new FrameBuffer();
		m_frameBuffer->init(1024, 1024,1,true, true);
		m_frameBuffer->gen();
		m_texture->setTextureId(m_frameBuffer->getTexture(0));
	
	}
} // namespace tzw
