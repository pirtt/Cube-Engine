#include "CubePlayer.h"
#include "EngineSrc/Scene/SceneMgr.h"
#include "EngineSrc/3D/Model/Model.h"
#include "GameConfig.h"
#include "GameWorld.h"
#include "EngineSrc/Event/EventMgr.h"
#include <iostream>
#include "3D/Primitive/CubePrimitive.h"
#include "3D/Primitive/SpherePrimitive.h"
#include "3D/Sky.h"
#include "Projectile.h"

#include "EngineSrc/Collision/PhysicsMgr.h"
#include "BuildingSystem.h"
#include "ItemMgr.h"
#include "AssistDrawSystem.h"
#include "Shader/ShaderMgr.h"
#include "PartSurfaceMgr.h"
#include "PaintGun.h"
#include "Base/TranslationMgr.h"
#include "3D/Primitive/RightPrismPrimitive.h"
#include "ButtonPart.h"
#include "SwitchPart.h"
#include "3D/Particle/ParticleEmitter.h"
#include "3D/Particle/ParticleInitSizeModule.h"
#include "3D/Particle/ParticleInitVelocityModule.h"
#include "3D/Particle/ParticleInitLifeSpanModule.h"
#include "3D/Particle/ParticleInitAlphaModule.h"
#include "3D/Particle/ParticleUpdateColorModule.h"
#include "3D/Particle/ParticleUpdateSizeModule.h"
#include "3D/Particle/ParticleInitPosModule.h"
#include "BulletMgr.h"

namespace tzw
{

	CubePlayer::CubePlayer(Node* mainRoot):m_seatViewMode(InSeatViewMode::ORBIT)
	{
		ItemMgr::shared();
		initSlots();
		m_paintGun = new PaintGun();
		m_paintGun->color = vec3(1, 1, 1);
		m_paintGun->m_surface = PartSurfaceMgr::shared()->getItem("foam grip");
		m_currMode = Mode::MODE_DEFORM_SPHERE;
		GUISystem::shared()->addObject(this);
		FPSCamera* camera = FPSCamera::create(g_GetCurrScene()->defaultCamera(), true);
		camera->setCamPos(vec3(5, 20.0, -5));
		mainRoot->addChild(camera);
		g_GetCurrScene()->setDefaultCamera(camera);
		m_camera = camera;
		camera->reCache();
		m_camera->setIsEnableGravity(true);
		m_currSelectItemIndex = 0;


		m_orbitcamera = OrbitCamera::create(g_GetCurrScene()->defaultCamera());
		m_fpvCamera = FPSCamera::create(g_GetCurrScene()->defaultCamera(), false);
		mainRoot->addChild(m_orbitcamera);

		auto pos = getPos();
		oldPosX = pos.x / ((MAX_BLOCK + 1) * BLOCK_SIZE);
		oldPosZ = (-1.0f * pos.z) / ((MAX_BLOCK + 1) * BLOCK_SIZE);
		Sky::shared()->setCamera(m_camera);
		EventMgr::shared()->addFixedPiorityListener(this);

		m_enableGravity = true;
		m_currPointPart = nullptr;

		m_previewItem = new PreviewItem();
		m_gunModel = Model::create("Models/Hammer.tzw");
		auto tex = TextureMgr::shared()->getByPath("Models/Hammer_DefaultMaterial_BaseColor.png", true);
		m_gunModel->getMat(0)->setTex("DiffuseMap", tex);
		auto metallicTexture =  TextureMgr::shared()->getByPath("Models/Hammer_DefaultMaterial_Metallic.png", true);
		m_gunModel->getMat(0)->setTex("MetallicMap", metallicTexture);

		auto roughnessTexture =  TextureMgr::shared()->getByPath("Models/Hammer_DefaultMaterial_Roughness.png", true);
		m_gunModel->getMat(0)->setTex("RoughnessMap", roughnessTexture);

		auto normalMapTexture =  TextureMgr::shared()->getByPath("Models/Hammer_DefaultMaterial_Normal.png", true);
		m_gunModel->getMat(0)->setTex("NormalMap", normalMapTexture);
		m_gunModel->setPos(0.09,0.5, -0.22);
		m_gunModel->setRotateE(vec3(0, 3, 0));
		m_gunModel->setIsAccpectOcTtree(false);
		m_camera->addChild(m_gunModel);
	}

	FPSCamera* CubePlayer::camera() const
	{
		return m_camera;
	}

	void CubePlayer::setCamera(FPSCamera* camera)
	{
		m_camera = camera;
	}

	vec3 CubePlayer::getPos()
	{
		return m_camera->getPos();
	}

	void CubePlayer::setPos(vec3 newPos)
	{
		m_camera->setPos(newPos);
	}

	void CubePlayer::logicUpdate(float dt)
	{
		static float theTime = 0.0f;
		vec3 oldPos = m_gunModel->getPos();
		float offset = 0.002;
		float freq = 1.2;
		if (m_camera->getIsMoving())
		{
			offset = 0.006;
			freq = 6;
		}
		theTime += freq * dt;
		m_gunModel->setPos(vec3(oldPos.x, -0.14 + sinf(theTime) * offset, oldPos.z));
		if (checkIsNeedUpdateChunk())
		{
			GameWorld::shared()->loadChunksAroundPlayer();
		}
		auto seat = BuildingSystem::shared()->getCurrentControlPart();
		if(seat && seat->getIsActivate()) 
		{
			auto label = GameUISystem::shared()->getCrossHairTipsInfo();
			if(!label) return;
			label->setIsVisible(false);
			return;
		}
		m_previewItem->handlePreview(ItemMgr::shared()->getItem(m_currSelectedItem), getPos(),m_camera->getTransform().forward());
		GamePart * part = nullptr;
		bool isInXray = BuildingSystem::shared()->isIsInXRayMode();
		if(isInXray)
		{
			part = BuildingSystem::shared()->rayTestPartXRay(getPos(), m_camera->getTransform().forward(), 10.0);
		}else
		{
			part = BuildingSystem::shared()->rayTestPart(getPos(), m_camera->getTransform().forward(), 10.0);
		}
		if(part)
		{
			if(m_currPointPart != part)
			{
				m_currPointPart = part;
				updateCrossHairTipsInfo();
			}
		}else
		{
			if(m_currPointPart)
			{
				m_currPointPart = nullptr;
				updateCrossHairTipsInfo();
			}
		}
	}

	bool CubePlayer::checkIsNeedUpdateChunk()
	{
		auto pos = getPos();
		int posX = pos.x / ((MAX_BLOCK + 1) * BLOCK_SIZE);
		int posZ = (-1.0f * pos.z) / ((MAX_BLOCK + 1) * BLOCK_SIZE);
		if (posX != oldPosX || posZ != oldPosZ)
		{
			oldPosX = posX;
			oldPosZ = posZ;
			return true;
		}
		return false;
	}
	static PhysicsRigidBody *wheelFrontLeft = nullptr;
	static PhysicsRigidBody *wheelFrontRight = nullptr;

	static PhysicsHingeConstraint * constraint1 = nullptr;
	static PhysicsHingeConstraint * constraint2 = nullptr;
	bool CubePlayer::onKeyPress(int keyCode)
	{
		switch (keyCode)
		{		
			case TZW_KEY_T:
			{	
			}
			break;
		default:
		  break;
		}
		return false;
	}

	bool CubePlayer::onKeyRelease(int keyCode)
	{
		tlog("is UI whant capture input %d",GUISystem::shared()->isUiCapturingInput());
		//if (MainMenu::shared()->isVisible()) return false;
		switch (keyCode)
		{
	    case TZW_KEY_ESCAPE:
	        GameUISystem::shared()->closeCurrentWindow();
	        break;
        case TZW_KEY_H:
			{
				auto m = getTransform().data();
				auto bullet = BulletMgr::shared()->fire(m_camera->getWorldPos(), m_camera->getForward(), 15, BulletType::Projecttile);
        		break;
        		if(!GUISystem::shared()->isUiCapturingInput())
				GameUISystem::shared()->setWindowShow(WindowType::QUICK_DEBUG, true);
        		
				auto anchorPos = getPos() + getForward() * 3.0;
        		for(int i = 0; i < 11; i++) {
					for(int j = 0; j < 11; j++)
					{
       					auto s = new SpherePrimitive(0.3, 25);
        				
						auto m = Material::createFromTemplate("ModelSimple");
						m->setVar("TU_roughness", float(i * 0.1));
						m->setVar("TU_metallic", float(j * 0.1));

						//m->setVar("TU_roughness", 0.0f);
						//m->setVar("TU_metallic", 1.0f);
						s->setPos(anchorPos + vec3( i * 1.0, 0, j * 1.0));
						s->setMaterial(m);
						g_GetCurrScene()->addNode(s);
						
					}
        		}

            }
			break;
        case TZW_KEY_TAB:
			{
        		if(!GUISystem::shared()->isUiCapturingInput())
				GameUISystem::shared()->setWindowShow(WindowType::MainMenu, !GameUISystem::shared()->getWindowIsShow(WindowType::MainMenu));
            }
			break;
        case TZW_KEY_X:
			{
				m_camera->setIsEnableGravity(!m_camera->getIsEnableGravity());
			}
			break;
		case TZW_KEY_T:
			{
				if(!GUISystem::shared()->isUiCapturingInput())
				{
					auto toggleXray = !BuildingSystem::shared()->isIsInXRayMode();
					BuildingSystem::shared()->setIsInXRayMode(toggleXray);
					AssistDrawSystem::shared()->setIsShowAssistInfo(toggleXray);
				}
			}
			break;
		case TZW_KEY_V:
			{
				if(getSeatViewMode() == InSeatViewMode::ORBIT) 
				{
					setSeatViewMode(InSeatViewMode::FPV);
				}
				else
				{
					setSeatViewMode(InSeatViewMode::ORBIT);
				}
				auto seat = BuildingSystem::shared()->getCurrentControlPart();
				if(seat && seat->getIsActivate()) 
				{
					handleSitDown();
                }
			}
			break;
		case TZW_KEY_F2:
			{
				ShaderMgr::shared()->reloadAllShaders();
			}
			break;
		default:
			break;
		}
		return false;
	}

	bool CubePlayer::onMousePress(int button, vec2 thePos)
	{
		if (GameUISystem::shared()->isVisible())
				return false;
		return true;
	}

	void CubePlayer::modeSwitch(Mode newMode)
	{
		m_currMode = newMode;
	}

	void CubePlayer::initSlots()
	{
		m_itemSlots.push_back(ItemMgr::shared()->getItem("Block"));
		m_itemSlots.push_back(ItemMgr::shared()->getItem("Wheel"));
		m_itemSlots.push_back(ItemMgr::shared()->getItem("Bearing"));
		m_itemSlots.push_back(ItemMgr::shared()->getItem("TerrainFormer"));
	}

	vec3 CubePlayer::getForward() const
	{
		return m_camera->getForward();
	}

	void CubePlayer::handleSitDown()
	{

		switch(m_seatViewMode)
		{
		case InSeatViewMode::FPV:
			g_GetCurrScene()->setDefaultCamera(m_fpvCamera);
		break;
		case InSeatViewMode::ORBIT:
			g_GetCurrScene()->setDefaultCamera(m_orbitcamera);
		break;
		default: ;
		}
	}
	
	void CubePlayer::sitDownToGamePart(GamePart * part)
	{
		m_camera->removeFromParent();
		m_camera->setEnableFPSFeature(false);
		m_camera->pausePhysics();
		tlog("setRotate fuck fuck%s",m_camera->getRotateE().getStr().c_str());
		//m_camera->setIsEnableGravity(false);
		//part->getNode()->addChild(m_camera);
		//m_camera->setPos(0, 0, 0);
		//m_camera->setRotateE(0, 0, 0);
		//m_camera->reCache();

		//fpv setup
		m_fpvCamera->setIsEnableGravity(false);
		part->getNode()->addChild(m_fpvCamera);
		auto Rotate = part->getNode()->getRotateE();
		m_fpvCamera->setPos(0, 0.5, 0.1);
		m_fpvCamera->setRotateE(0, 180, 0);
		m_fpvCamera->reCache();
		
		//orbit setup
		m_orbitcamera->resetDirection();
		m_orbitcamera->setFocusNode(part->getNode());

		handleSitDown();
	}

	void CubePlayer::standUpFromGamePart(GamePart * part)
	{
		//leave
		m_camera->setIsEnableGravity(true);
		m_camera->setEnableFPSFeature(true);
		m_camera->resumePhysics();
		auto aabb = part->m_parent->getAABBInWorld();
		auto dist = aabb.half().z;
		m_camera->setCamPos(aabb.centre() + vec3(0,0,-1) * (dist + 0.5));
		m_camera->lookAt(part->getWorldPos());
		tlog("fuck fuck fuck%s",m_camera->getRotateE().getStr().c_str());
		GameWorld::shared()->getMainRoot()->addChild(m_camera);
		m_orbitcamera->setFocusNode(nullptr);

		m_fpvCamera->removeFromParent();
		
		g_GetCurrScene()->setDefaultCamera(m_camera);
	}

	void CubePlayer::removePartByAttach(Attachment* attach)
	{
		BuildingSystem::shared()->removePartByAttach(attach);
		m_currPointPart = nullptr;
		updateCrossHairTipsInfo();
	}

	void CubePlayer::removePart(GamePart* part)
	{
		BuildingSystem::shared()->removePart(part);
		m_currPointPart = nullptr;
		updateCrossHairTipsInfo();
	}

	void CubePlayer::removeAllBlocks()
	{
		BuildingSystem::shared()->removeAll();
		m_currPointPart = nullptr;
	}

	void CubePlayer::updateCrossHairTipsInfo()
	{
		auto label = GameUISystem::shared()->getCrossHairTipsInfo();
		if(!label) return;
		auto item = ItemMgr::shared()->getItem(m_currSelectedItem);
		bool isNeedSpecialShowBySelected = false;
		if(item && item->isSpecialFunctionItem())
		{
			isNeedSpecialShowBySelected = true;
			switch(item->m_type)
			{
			case GamePartType::GAME_PART_LIFT:
				if(!BuildingSystem::shared()->getLift())
				{
					//如果当前指向的方块有属性面板，也不显示
					if(m_currPointPart && m_currPointPart->getItem()->hasAttributePanel())
					{
						isNeedSpecialShowBySelected = false;
					} else
					{
						isNeedSpecialShowBySelected = true;
					}
				}
				else //已经放置了不需要再显示
				{
					isNeedSpecialShowBySelected = false;
				}
			break;
			case GamePartType::SPECIAL_PART_PAINTER: isNeedSpecialShowBySelected = true; break;
			default: ;
			}
		}else
		{
			isNeedSpecialShowBySelected = false;
		}

		if(isNeedSpecialShowBySelected)
		{
			label->setIsVisible(true);
			switch(item->m_type)
			{
			case GamePartType::GAME_PART_LIFT:
				if(!BuildingSystem::shared()->getLift())
				{
					if(!BuildingSystem::shared()->getStoreIslandGroup().empty()) 
					{
						label->setString(TR(u8"放置收纳对象"));
					}
					else
					{
						if(m_currPointPart)
						{
							label->setString(TR(u8"收纳"));
						}
						else
						{
							label->setString(TR(u8"对准空地放置，对准载具收纳放置"));
						}
					}
				}
			break;
			case GamePartType::SPECIAL_PART_PAINTER: label->setString(TR(u8"(左键) 喷漆 \n(右键) 喷涂面板")); break;
			default: ;
			}
		}else
		{
			if(!m_currPointPart) 
			{
				label->setIsVisible(false);
				return;
	        }
			label->setIsVisible(true);
			switch(m_currPointPart->getType())
			{
			case GamePartType::GAME_PART_LIFT: label->setString(TR(u8"(E) 载具浏览器")); break;
			case GamePartType::GAME_PART_CONTROL: label->setString(TR(u8"(E) 驾驶\n(F) 节点编辑器"));break;
			case GamePartType::GAME_PART_THRUSTER: label->setString(TR(u8"(E) 属性面板"));break;
			case GamePartType::GAME_PART_CANNON: label->setString(TR(u8"(E) 属性面板"));break;
			case GamePartType::GAME_PART_BEARING: label->setString(TR(u8"(E) 属性面板\n(F) 调整方向"));break;
			case GamePartType::GAME_PART_SPRING: label->setString(TR(u8"(E) 属性面板"));break;
			default:
				label->setIsVisible(false);
				break;
			}
		}
	}

	void CubePlayer::openCurrentPartInspectMenu()
	{
		if(m_currPointPart && m_currPointPart->isNeedDrawInspect())
		{
			GameUISystem::shared()->openInspectWindow(m_currPointPart);
		}
	}

	GamePart* CubePlayer::getCurrPointPart()
	{
		return m_currPointPart;
	}

	PaintGun* CubePlayer::getPaintGun()
	{
		return m_paintGun;
	}

	void CubePlayer::paint()
	{
		auto part = BuildingSystem::shared()->rayTestPart(getPos(), m_camera->getTransform().forward(), 10.0);
		if(part)
		{
			m_paintGun->paint(part);
		}
	}

	void CubePlayer::setCurrSelected(std::string itemName)
	{
		updateCrossHairTipsInfo();
		if(itemName.empty()) return;
		m_currSelectedItem = itemName;
		if(itemName == "Painter") return;
		m_previewItem->setPreviewItem(ItemMgr::shared()->getItem(itemName));

	}

	void CubePlayer::setPreviewAngle(float angle)
	{
		m_previewItem->setPreviewAngle(angle);
		tlog("%f", angle);
	}

	float CubePlayer::getPreviewAngle() const
	{
		return m_previewItem->getPreviewAngle();
	}

	PreviewItem* CubePlayer::getPreviewItem()
	{
		return m_previewItem;
	}

	void CubePlayer::pressButton(GamePart* buttonPart)
	{
		tlog("press Button!", buttonPart);
		static_cast<ButtonPart *>(buttonPart)->onPressed();
		
	}

	void CubePlayer::releaseButton(GamePart* buttonPart)
	{
		tlog("release Button!", buttonPart);
		static_cast<ButtonPart *>(buttonPart)->onReleased();
	}

	void CubePlayer::releaseSwitch(GamePart* switchPart)
	{
		tlog("release Button!", switchPart);
		static_cast<SwitchPart *>(switchPart)->onToggle();
	}

	CubePlayer::InSeatViewMode CubePlayer::getSeatViewMode() const
	{
		return m_seatViewMode;
	}

	void CubePlayer::setSeatViewMode(const InSeatViewMode seatViewMode)
	{
		m_seatViewMode = seatViewMode;
	}

	void CubePlayer::setIsOpenJetPack(bool isOpen)
	{
		m_camera->setIsEnableGravity(!isOpen);
	}

	bool CubePlayer::onScroll(vec2 offset)
	{
		tlog("offset %f %f", offset.getX(), offset.getY());
		m_orbitcamera->zoom(-1.f * offset.getY() * 0.25);
		return true;
	}

	GameItem* CubePlayer::getCurSelectedItem()
	{
		return ItemMgr::shared()->getItem(m_currSelectedItem);
	}
} // namespace tzw
