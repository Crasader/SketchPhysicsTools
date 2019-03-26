#include "CanvasScene.h"
#include "SceneManager.h"
#include "controller/PawnController.h"
#include "math.h"
#include "resource/Resources.h"
#include "resource/ToolHintConstants.h"

USING_NS_CC;
using namespace DollarRecognizer;

// gloable join list
Joints		joints;
JointsList	jointsList;

namespace
{
	const int DRAG_BODYS_TAG = 0x80;
	const Vec2 GRAVITY(0, -100);
}

CanvasScene::CanvasScene()
:_debugDraw(false)
{
}

bool CanvasScene::toggleDebugDraw()
{
#if CC_USE_PHYSICS
	_debugDraw = !_debugDraw;
	_physicsWorld->setDebugDrawMask(_debugDraw ? PhysicsWorld::DEBUGDRAW_ALL : PhysicsWorld::DEBUGDRAW_NONE);
#endif
	_toolLayer->toggleDebugMode(_debugDraw);
	return _debugDraw;
}

bool CanvasScene::init()
{
	// create scene with physics
	if (!Scene::initWithPhysics())
	{
		return false;
	}

	// add GameCanvasLayer
	_canvasLayer = GameCanvasLayer::create();
	this->addChild(_canvasLayer);

	// add ToolLayer
	_toolLayer = ToolLayer::create();
	_toolLayer->setParentScene(this);
	this->addChild(_toolLayer);
	return true;
}

void CanvasScene::onEnter()
{
	Scene::onEnter();
	_physicsWorld->setGravity(GRAVITY);

	// initialize keyboard listener
	_keyboardListener = EventListenerKeyboard::create();
	_keyboardListener->onKeyPressed = [&](EventKeyboard::KeyCode keyCode, Event * event){
		log("KeyPress:%d", keyCode);

		if (EventKeyboard::KeyCode::KEY_ENTER == keyCode)
		{
			this->_canvasLayer->recognize();
		}
		else if (EventKeyboard::KeyCode::KEY_D == keyCode)
		{
			this->_canvasLayer->redrawCurrentNode();
		}
		else if (EventKeyboard::KeyCode::KEY_C == keyCode)
		{
			if (!jointsList.empty())jointsList.pop_back();
		}
	};
	_keyboardListener->onKeyReleased = [](EventKeyboard::KeyCode keyCode, Event * event){
		log("KeyRelease:%d", keyCode);
	};

	// register keyboard listener to event dispatcher
	_eventDispatcher->addEventListenerWithSceneGraphPriority(_keyboardListener, this);

}

void CanvasScene::onExit()
{
	// stop all actions/animations
	this->stopAllActions();

	// remove event listener
	_eventDispatcher->removeEventListenersForTarget(this);

	// call super onExit
	Scene::onExit();
}

void CanvasScene::startSimulate()
{
	_gameLayer = _canvasLayer->createGameLayer();
	_gameLayer->setVisible(true);

	
	_canvasLayer->startGameSimulation();
	_canvasLayer->setVisible(false);
	_canvasLayer->stopAllActions();
	if (this->_debugDraw){ _physicsWorld->setDebugDrawMask(PhysicsWorld::DEBUGDRAW_ALL); }
	_gameLayer->setParent(nullptr);
	this->addChild(_gameLayer);
}

void CanvasScene::stopSimulate()
{
	_gameLayer->setVisible(false);
	_canvasLayer->setVisible(true);
	
	// stop all actions/animations
	_gameLayer->stopAllActions();
	
	// turn off debug view
	if (this->_debugDraw){ _physicsWorld->setDebugDrawMask(PhysicsWorld::DEBUGDRAW_NONE); }
	
	// dettach game layer
	this->removeChild(_gameLayer);

	_canvasLayer->stopGameSimulation();
}

bool GameCanvasLayer::init()
{
	if (!CanvasLayer::init())
	{
		return false;
	}

	// initialize loadTemplateListener, listen to EVENT_LOADED_TEMPLATE
	auto _loadedTemplateListener = EventListenerCustom::create(EVENT_LOADED_TEMPLATE, [&](EventCustom* event){
		_keyboardListener->setEnabled(true);
		_mouseListener->setEnabled(true);
	});
	_eventDispatcher->addEventListenerWithSceneGraphPriority(_loadedTemplateListener, this);

	// add GeometricRecognizerNode
	_geoRecognizer = GeometricRecognizerNode::create();
	this->addChild(_geoRecognizer);

	// initialize pre-command handler
	_preCmdHandlers.init();
	
	return true;
}

void GameCanvasLayer::onEnter()
{
	CanvasLayer::onEnter();

	// create a new draw node, and switch to it
	_currentDrawNode = switchToNewDrawNode();

	// create keyboard listener for GameCanvasLayer
	_keyboardListener = EventListenerKeyboard::create();
	_keyboardListener->onKeyPressed = [&](EventKeyboard::KeyCode keyCode, Event * event){
		if (EventKeyboard::KeyCode::KEY_F5 == keyCode)
		{
			((CanvasScene*)(this->getScene()))->toggleDebugDraw();
		}
		else if (EventKeyboard::KeyCode::KEY_CTRL == keyCode)
		{
			this->_jointMode = true;
			joints.clear();
		}
		else if (EventKeyboard::KeyCode::KEY_ESCAPE == keyCode)
		{
			auto scene = SceneManager::GetMenuScene();
			Director::getInstance()->replaceScene(scene);
		}
	};
	_keyboardListener->onKeyReleased = [&](EventKeyboard::KeyCode keyCode, Event * event){
		if (EventKeyboard::KeyCode::KEY_CTRL == keyCode)
		{
			this->_jointMode = false;
		}
	};

	// disable listener in initial
	_keyboardListener->setEnabled(false);
	_mouseListener->setEnabled(false);
	_eventDispatcher->addEventListenerWithSceneGraphPriority(_keyboardListener, this);

	// uncomment when you want to enable debug mode when program start
	// ((CanvasScene*)(this->getScene()))->toggleDebugDraw();
}

void GameCanvasLayer::onExit()
{
	// remove event listener
	_eventDispatcher->removeEventListenersForTarget(this);

	// call super onExit
	Layer::onExit();
}

void GameCanvasLayer::onMouseDown(cocos2d::EventMouse* event)
{
	// if joint mode is enabled
	if (_jointMode)
	{
		for (auto p = _drawNodeResultMap.begin(); p != _drawNodeResultMap.end(); p++)
		{
			auto& rs = p->second;
			if (rs._priority == 5)
			{
				if (rs._drawNode->containsPoint(event->getLocationInView()))
				{
					joints.push_back(rs._drawNode);
					break;
				}
			}
		}
		return;
	}
	CanvasLayer::onMouseDown(event);
}

void GameCanvasLayer::onMouseUp(cocos2d::EventMouse* event){
	CanvasLayer::onMouseUp(event);
	this->recognize();
}

DrawableSprite* GameCanvasLayer::switchToNewDrawNode()
{
	auto drawNode = DrawableSprite::create();
	drawNode->setGeoRecognizer(this->_geoRecognizer->getGeometricRecognizer());
	this->addChild(drawNode, 10);
	this->_drawNodeList.push_back(drawNode);
	return drawNode;
}

void GameCanvasLayer::startGameSimulation()
{
	_mouseListener->setEnabled(false);
}

void GameCanvasLayer::stopGameSimulation()
{
	_mouseListener->setEnabled(true);
}

void GameCanvasLayer::removeUnrecognizedSprite(DrawableSprite* target)
{
	// play blink animation when sprite is not recognized
	// @see cocos2d::Blink
	auto actionBlink = Blink::create(1.5f, 3); 
	auto actionDone = CallFuncN::create([&](Node* node)
	{
		// remove unrecognized sprite
		this->removeChild(node);
	});
	Sequence* sequence = Sequence::create(actionBlink, actionDone, NULL);
	target->runAction(sequence);
}

void GameCanvasLayer::recognize()
{
	if (_jointMode)
	{
		//joints.push_back(joints[0]);
		jointsList.push_back(joints);
		joints.clear();
		return;
	}
	RecognitionResult result = _currentDrawNode->recognize();
	RecognizedSprite rs(result, _currentDrawNode);
	if (result.score < 0.75f)
	{
		log("Geometric Recognize Failed. Guess: %s, Score: %f", result.name.c_str(), result.score);
		EventCustom event(EVENT_RECOGNIZE_FAILED);
		event.setUserData((void*)&rs);
		_eventDispatcher->dispatchEvent(&event);
		removeUnrecognizedSprite(_currentDrawNode);
	}
	else
	{
		CommandHandler cmdh = _preCmdHandlers.getCommandHandler(rs.getGeometricType());
		if (!cmdh._Empty())cmdh(rs, _drawNodeList, this, &_drawNodeResultMap);

		// dispatch success event
		EventCustom event(EVENT_RECOGNIZE_SUCCESS);
		event.setUserData((void*)&rs);
		_eventDispatcher->dispatchEvent(&event);
	}
	_currentDrawNode = switchToNewDrawNode();
}

GameLayer* GameCanvasLayer::createGameLayer()
{
	return GameLayer::create(this->_drawNodeList, this->_drawNodeResultMap, this->getScene());
}

void GameCanvasLayer::redrawCurrentNode()
{
	_currentDrawNode->redraw();
}

bool ToolLayer::init()
{
	if (!Layer::init())
	{
		return false;
	}

#define GAP 20.0f 
	Size visibleSize = Director::getInstance()->getVisibleSize();
	Vec2 origin = Director::getInstance()->getVisibleOrigin();

	// assistant
	_spriteAssistant = Sprite::create(RES_IMAGE(assistant.png));
	_spriteAssistant->setScale(0.5f);
	_spriteAssistant->setPosition(Vec2(origin.x + _spriteAssistant->getContentSize().width / 4 + GAP,
		origin.y + visibleSize.height - _spriteAssistant->getContentSize().height / 4 - GAP));
	this->addChild(_spriteAssistant);

	// hint label
	_labelHint = Label::create(TOOL_HINT_WELCOME, DEFAULT_FONT, 24, Size::ZERO, TextHAlignment::LEFT, TextVAlignment::CENTER);
	_labelHint->setAnchorPoint(Vec2(0, 0));
	_labelHint->setPosition(Vec2(origin.x + _spriteAssistant->getContentSize().width / 2 + 4 * GAP,
		origin.y + visibleSize.height - _spriteAssistant->getContentSize().height / 4 - GAP));
	//this->addChild(_labelHint);

	// add back label
	auto _labelBack = Label::create("Back", "fonts/Marker Felt.ttf", 32, Size::ZERO, TextHAlignment::LEFT, TextVAlignment::CENTER);
	auto backItem = MenuItemLabel::create(_labelBack, [](Ref *pSender) {
		auto scene = SceneManager::GetMenuScene();
		Director::getInstance()->replaceScene(scene);
	});
	backItem->setPosition(Vec2(visibleSize.width + origin.x - 100, origin.y + 90));
	auto menu1 = Menu::create(backItem, nullptr);
	menu1->setPosition(Vec2::ZERO);
	this->addChild(menu1);

	// add a "start simulate/stop simulate" icon to enter/exit the progress. it's an autorelease object
	_menuStartSimulate = MenuItemImage::create(
		RES_IMAGE(StartSimulate.png),
		RES_IMAGE(StartSimulateHover.png),
		CC_CALLBACK_1(ToolLayer::startSimulateCallback, this));
	_menuStartSimulate->setScale(0.5f);
	_menuStartSimulate->setRotation(90.0f);
	_menuStartSimulate->setPosition(Vec2(origin.x + visibleSize.width - _menuStartSimulate->getContentSize().width / 2,
		origin.y + visibleSize.height - _menuStartSimulate->getContentSize().height / 2));
	_menuStartSimulate->setVisible(true);

	_menuStopSimulate = MenuItemImage::create(
		RES_IMAGE(StopSimulate.png),
		RES_IMAGE(StopSimulateHover.png),
		CC_CALLBACK_1(ToolLayer::stopSimulateCallback, this));
	_menuStopSimulate->setScale(0.5f);
	_menuStopSimulate->setPosition(Vec2(origin.x + visibleSize.width - _menuStopSimulate->getContentSize().width / 2,
		origin.y + visibleSize.height - _menuStopSimulate->getContentSize().height / 2));
	_menuStopSimulate->setVisible(false);
	
	// add debug menu
	_menuStartDebug = MenuItemImage::create(
		RES_IMAGE(DebugMode.png),
		RES_IMAGE(DebugMode.png));
	_menuStartDebug->setScale(0.5f);
	_menuStartDebug->setPosition(Vec2(origin.x + visibleSize.width - _menuStartDebug->getContentSize().width / 2,
		origin.y + visibleSize.height - _menuStartDebug->getContentSize().height / 2));
	_menuStartDebug->setVisible(false);

	// create menu, it's an autorelease object
	auto menu = Menu::create(_menuStartSimulate, _menuStopSimulate, _menuStartDebug, NULL);
	menu->setPosition(Vec2::ZERO);
	this->addChild(menu, 1);

	return true;
}

void ToolLayer::onRecognizeSuccess(EventCustom* event)
{
	RecognizedSprite* result = static_cast<RecognizedSprite*>(event->getUserData());
	char buf[100];
	GetToolHintRecognizeSucc(buf, result->getGeometricType().c_str(), result->_result.score);
	this->_labelHint->setString(buf);
}

void ToolLayer::onRecognizeFailed(EventCustom* event)
{
	RecognizedSprite* result = static_cast<RecognizedSprite*>(event->getUserData());
	char buf[100];
	GetToolHintRecognizeFailed(buf, result->getGeometricType().c_str(), result->_result.score);
	this->_labelHint->setString(buf);
}

void ToolLayer::onEnter()
{
	Layer::onEnter();

	/**
	// initialize listeners
	_recognizeSuccessListener = EventListenerCustom::create(EVENT_RECOGNIZE_SUCCESS, CC_CALLBACK_1(ToolLayer::onRecognizeSuccess, this));
	_recognizeFailedListener = EventListenerCustom::create(EVENT_RECOGNIZE_FAILED, CC_CALLBACK_1(ToolLayer::onRecognizeFailed, this));

	_eventDispatcher->addEventListenerWithSceneGraphPriority(_recognizeSuccessListener, this);
	_eventDispatcher->addEventListenerWithSceneGraphPriority(_recognizeFailedListener, this);


	auto _loadTemplateListener = EventListenerCustom::create(EVENT_LOADING_TEMPLATE, [&](EventCustom* event){
		LoadTemplateData* ltd = static_cast<LoadTemplateData*>(event->getUserData());
		this->_labelHint->setString(ltd->_hint.c_str());
	});
	auto _loadedTemplateListener = EventListenerCustom::create(EVENT_LOADED_TEMPLATE, [&](EventCustom* event){
		LoadTemplateData* ltd = static_cast<LoadTemplateData*>(event->getUserData());
		this->_labelHint->setString(ltd->_hint.c_str());
	});


	// register listeners
	_eventDispatcher->addEventListenerWithSceneGraphPriority(_loadTemplateListener, this);
	_eventDispatcher->addEventListenerWithSceneGraphPriority(_loadedTemplateListener, this);
	*/
}

void ToolLayer::onExit()
{
	// remove listeners
	_eventDispatcher->removeEventListenersForTarget(this);
	Layer::onExit();
}

void ToolLayer::startSimulateCallback(cocos2d::Ref* pSender)
{
	_menuStartSimulate->setVisible(false);
	_menuStopSimulate->setVisible(true);
	_canvasScene->startSimulate();
	auto physicsBody = _canvasScene->getPhysicsWorld()->getBody(RECTANGLE_TAG);
	log("speed: %d", physicsBody);
}


void ToolLayer::stopSimulateCallback(cocos2d::Ref* pSender)
{
	_menuStartSimulate->setVisible(true);
	_menuStopSimulate->setVisible(false);
	_canvasScene->stopSimulate();

}

void ToolLayer::startJointModeCallback(cocos2d::Ref* pSender)
{
	_menuStartJoint->setVisible(false);
	_menuStopJoint->setVisible(true);
}

void ToolLayer::stopJointModeCallback(cocos2d::Ref* pSender)
{
	_menuStartJoint->setVisible(true);
	_menuStopJoint->setVisible(false);
}

void ToolLayer::toggleDebugMode(bool isDebug)
{
	_menuStartDebug->setVisible(isDebug);
}

bool GameLayer::init()
{
	if (!Layer::init())
	{
		return false;
	}

	// init post-command handler
	_postCmdHandlers.init();

	// init Geometric physics body collison mask
	InitGeometricPhysicsMask();

	//init drawvelocityLayer
	_drawVelocityLayer = DrawVelocityLayer::create();

	// put recognized sprite into priority queue
	priority_queue<RecognizedSprite*, vector<RecognizedSprite*>, RecognizedSpriteLessCmp> lazyQueue;
	for (auto p = _drawNodeResultMap.begin(); p != _drawNodeResultMap.end(); p++)
	{
		auto ds = p->first;
		auto rs = &(p->second);
		if (!ds->empty() && rs->_priority >= 0){ lazyQueue.push(rs); }
	}

	// fetch recognized sprite from priority queue according to sprite priority
	// generate sprite with physics body with post-command handler
	while (!lazyQueue.empty())
	{
		auto rs = lazyQueue.top();
		lazyQueue.pop();
		auto cmdh = _postCmdHandlers.getCommandHandler(rs->getGeometricType());
		if (!cmdh._Empty()) cmdh(*rs, _drawNodeList, this, &_genSpriteResultMap);
	}

	_begin_move = clock();
	log("init time:%f", (double)_begin_move);
	_drawVelocityLayer->setVisible(true);

	// add concat listener
	auto contactListener = EventListenerPhysicsContact::create();
	contactListener->onContactPreSolve = CC_CALLBACK_1(GameLayer::onPhysicsContactBegin, this);
	_eventDispatcher->addEventListenerWithSceneGraphPriority(contactListener, this);

	// add draw velocity layer
	this->addChild(_drawVelocityLayer);
	_drawVelocityLayer->setParent(this);
	_drawVelocityLayer->setVisible(true);
	log("game layer init");

	return true;
}

GameLayer *GameLayer::create(list<DrawableSprite*>& drawNodeList, DrawSpriteResultMap& drawNodeResultMap, Scene* scene)
{
	GameLayer *ret = new (std::nothrow) GameLayer(drawNodeList, drawNodeResultMap);
	ret->setParent(scene);
	if (ret && ret->init())
	{
		ret->autorelease();
		return ret;
	}
	else
	{
		CC_SAFE_DELETE(ret);
		return nullptr;
	}
}

GameLayer::GameLayer(list<DrawableSprite*>& drawNodeList, DrawSpriteResultMap& drawNodeResultMap)
	:_drawNodeList(drawNodeList)
	, _drawNodeResultMap(drawNodeResultMap)
{}

void GameLayer::onEnter()
{
	Layer::onEnter();
	log("game layer on enter");
	/**
	Device::setAccelerometerEnabled(true);//打开设备的重力感应
	auto listener = EventListenerAcceleration::create(CC_CALLBACK_2(GameLayer::onAcceleration, this));//创建一个重力监听事件
	_eventDispatcher->addEventListenerWithSceneGraphPriority(listener, this);//将listener放到事件委托中
	*/
	this->schedule(schedule_selector(GameLayer::updateVelocityText), 0.1);
	_postCmdHandlers.makeJoints(this->getScene()->getPhysicsWorld(), jointsList, _genSpriteResultMap);
}

void GameLayer::updateVelocityText(float t)
{
	clock_t now = clock();
	auto duration = (double)(now - _begin_move) / CLOCKS_PER_SEC;
	int index = 0;
	for (auto i = _genSpriteResultMap.begin(); i != _genSpriteResultMap.end(); i++)
		{
			
			auto sprite = i->second;
			auto cur = sprite->getPhysicsBody();
			Vec2 vec = cur->getVelocity();
			if (cur->isDynamic()){
				log("get Tag:%d", cur->getTag());
				log("body:%f, %f", vec.x, vec.y);
				_drawVelocityLayer->drawVelocityLine(vec, duration, index);
				index++;
			}		
		}
}

void GameLayer::onExit()
{
	_drawVelocityLayer->setVisible(false);
	this->stopAllActions();
	_eventDispatcher->removeEventListenersForTarget(this);
	Layer::onExit();

}

void GameLayer::recordVelocityCallBack(cocos2d::Ref* pSender)
{
	log("nothing");
}

bool GameLayer::onPhysicsContactBegin(const cocos2d::PhysicsContact& contact){
	log("enter game layer listener:===============");
	clock_t collison_time = clock();
	auto a = contact.getShapeA()->getBody();
	auto b = contact.getShapeB()->getBody();
	contact.getShapeA()->setFriction(0.5);
	contact.getShapeB()->setFriction(0.5);
	auto duration = (double)(collison_time - _begin_move) / CLOCKS_PER_SEC;
	if (b->isDynamic()){
		Vec2 vec = b->getVelocity();
		log("vel:(%f,%f)", vec.x, vec.y);
		log("tag 1:%d", b->getTag());
		//_drawVelocityLayer->drawVelocityLine(vec, duration, 0);
	}
	else{
		Vec2 vec = a->getVelocity();
		log("vel:(%f,%f)", vec.x, vec.y); 
		log("tag 1:%d", b->getTag());
		//_drawVelocityLayer->drawVelocityLine(vec, duration, 0);
	}
	return true;
}

bool DrawVelocityLayer::init(){
	if (!CanvasLayer::init())
	{
		return false;
	}
	// gesture
	_gestureBackgroundView = ui::ImageView::create("v_t_background.png");
	_gestureBackgroundView->setContentSize(Size(450, 450));
	_gestureBackgroundView->setScale9Enabled(true);
	_gestureBackgroundView->setPosition(Vec2(ZERO_POINT_X, 0));
	addChild(_gestureBackgroundView);
	_startDrawLineLocation = Vec2(ZERO_POINT_X, ZERO_POINT_Y);
	log("enter velocity layer init");
	return true;
}

void DrawVelocityLayer::onEnter(){
	currentDrawLine = DrawableSprite::create();
	this->addChild(currentDrawLine);
	log("enter velocity layer");
}

void DrawVelocityLayer::onExit(){
	log("exit velocity layer");
}

void DrawVelocityLayer::drawVelocityLine(Vec2 velocity, double t, int index){
	auto absolute_velocity = sqrt(pow(velocity.x, 2) + pow(velocity.y, 2));
	auto value = _startDrawLineMap.find(index);
	Vec2 currentLocation = Vec2(t * 10 + ZERO_POINT_X, absolute_velocity + ZERO_POINT_Y);
	if (value != _startDrawLineMap.end()){
		auto startDrawLineLocation = _startDrawLineMap.find(index)->second;
		currentDrawLine->drawLine(startDrawLineLocation, currentLocation, currentDrawLine->getBrushColor());
		_startDrawLineMap.erase(value);	
	}
	else{
		currentDrawLine->drawLine(_startDrawLineLocation, currentLocation, currentDrawLine->getBrushColor());
	}
	_startDrawLineMap[index] = currentLocation;
	
	log("absolute_velocity:%f, time: %f", absolute_velocity, t * 10);
	//log("start location:%f,%f ", _startDrawLineLocation.x, _startDrawLineLocation.y);
}