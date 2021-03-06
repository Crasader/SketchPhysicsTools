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

	_gameLayer->init_v_x = _toolLayer->init_v_x;
	_gameLayer->init_v_y = _toolLayer->init_v_y;
	_gameLayer->init_friction = _toolLayer->init_friction;

	_gameLayer->init_f_x = _toolLayer->init_f_x;
	_gameLayer->init_f_y = _toolLayer->init_f_y;
	_gameLayer->initVelocityForPhysicsBody();
	_gameLayer->initForceForPhysicsBody();
	//_toolLayer->updateTextFieldState(false);
	log("game layer simluation: %d, %d", _gameLayer->init_friction.size(), _toolLayer->init_friction.size());
	_canvasLayer->startGameSimulation();
	_canvasLayer->setVisible(false);
	_canvasLayer->stopAllActions();
	if (this->_debugDraw){ _physicsWorld->setDebugDrawMask(PhysicsWorld::DEBUGDRAW_ALL); }
	_gameLayer->setParent(nullptr);
	this->addChild(_gameLayer);
	// start clock
	_gameLayer->_begin_move = clock();
}

void CanvasScene::stopSimulate()
{
	_gameLayer->setVisible(false);
	_canvasLayer->setVisible(true);
	
	// stop all actions/animations
	_gameLayer->stopAllActions();
	_gameLayer->init_v_x.clear();
	_gameLayer->init_v_y.clear();
	_gameLayer->init_friction.clear();
	_gameLayer->init_f_x.clear();
	_gameLayer->init_f_y.clear();
	//_toolLayer->updateTextFieldState(true);
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
	/**
	_labelHint = Label::create(TOOL_HINT_WELCOME, DEFAULT_FONT, 24, Size::ZERO, TextHAlignment::LEFT, TextVAlignment::CENTER);
	_labelHint->setAnchorPoint(Vec2(0, 0));
	_labelHint->setPosition(Vec2(origin.x + _spriteAssistant->getContentSize().width / 2 + 4 * GAP,
		origin.y + visibleSize.height - _spriteAssistant->getContentSize().height / 4 - GAP));
		*/
	//this->addChild(_labelHint);

	// input v_x
	auto _VxField = cocos2d::ui::TextField::create("Vx:0", DEFAULT_FONT, 24);
	_VxField->setPosition(Vec2(origin.x + _spriteAssistant->getContentSize().width / 2 + 4 * GAP,
		origin.y + visibleSize.height - _spriteAssistant->getContentSize().height / 4 - GAP));
	_VxField->addEventListener(CC_CALLBACK_2(ToolLayer::InputVxEvent, this));
	this->addChild(_VxField);

	// input v_y
	auto _VyField = cocos2d::ui::TextField::create("Vy:0", DEFAULT_FONT, 24);
	_VyField->setPosition(Vec2(origin.x + _spriteAssistant->getContentSize().width / 2 + 10 * GAP,
		origin.y + visibleSize.height - _spriteAssistant->getContentSize().height / 4 - GAP));
	_VyField->addEventListener(CC_CALLBACK_2(ToolLayer::InputVyEvent, this));
	this->addChild(_VyField);

	// input friction
	_FrictionField = cocos2d::ui::TextField::create("Friction:0", DEFAULT_FONT, 24);
	_FrictionField->setPosition(Vec2(origin.x + _spriteAssistant->getContentSize().width / 2 + 16 * GAP,
		origin.y + visibleSize.height - _spriteAssistant->getContentSize().height / 4 - GAP));
	_FrictionField->addEventListener(CC_CALLBACK_2(ToolLayer::InputFrictionEvent, this));
	this->addChild(_FrictionField);

	// input f_x
	_FxField = cocos2d::ui::TextField::create("Fx:0", DEFAULT_FONT, 24);
	_FxField->setPosition(Vec2(origin.x + _spriteAssistant->getContentSize().width / 2 + 22 * GAP,
		origin.y + visibleSize.height - _spriteAssistant->getContentSize().height / 4 - GAP));
	_FxField->addEventListener(CC_CALLBACK_2(ToolLayer::InputFxEvent, this));
	this->addChild(_FxField);

	// input f_y
	_FyField = cocos2d::ui::TextField::create("Fy:0", DEFAULT_FONT, 24);
	_FyField->setPosition(Vec2(origin.x + _spriteAssistant->getContentSize().width / 2 + 28 * GAP,
		origin.y + visibleSize.height - _spriteAssistant->getContentSize().height / 4 - GAP));
	_FyField->addEventListener(CC_CALLBACK_2(ToolLayer::InputFyEvent, this));
	this->addChild(_FyField);


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
	
}


void ToolLayer::stopSimulateCallback(cocos2d::Ref* pSender)
{
	_menuStartSimulate->setVisible(true);
	_menuStopSimulate->setVisible(false);
	_canvasScene->stopSimulate();

	/**
	this->init_v_x.clear();
	this->init_v_y.clear();
	this->init_friction.clear();
	*/
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

void ToolLayer::InputVxEvent(Ref *pSender, cocos2d::ui::TextField::EventType type){
	cocos2d::ui::TextField* textField1 = dynamic_cast<cocos2d::ui::TextField*>(pSender);
	std::string v_x;
	
	switch (type)
	{
	case cocos2d::ui::TextField::EventType::ATTACH_WITH_IME:
		break;
	case cocos2d::ui::TextField::EventType::DETACH_WITH_IME:
		init_v_x.clear();
		this->change_input_to_array(textField1->getString(), init_v_x);
		break;
	case cocos2d::ui::TextField::EventType::INSERT_TEXT:
		break;
	case cocos2d::ui::TextField::EventType::DELETE_BACKWARD:
		break;
	default:
		break;
	}
}
void ToolLayer::InputVyEvent(Ref *pSender, cocos2d::ui::TextField::EventType type){
	cocos2d::ui::TextField* textField2 = dynamic_cast<cocos2d::ui::TextField*>(pSender);
	switch (type)
	{
	case cocos2d::ui::TextField::EventType::ATTACH_WITH_IME:
		break;
	case cocos2d::ui::TextField::EventType::DETACH_WITH_IME:
		log("Vy: %s", textField2->getString());
		init_v_y.clear();
		this->change_input_to_array(textField2->getString(), init_v_y);
		break;
	case cocos2d::ui::TextField::EventType::INSERT_TEXT:
		break;
	case cocos2d::ui::TextField::EventType::DELETE_BACKWARD:
		break;
	default:
		break;
	}
}
void ToolLayer::InputFrictionEvent(Ref *pSender, cocos2d::ui::TextField::EventType type){
	cocos2d::ui::TextField* textField3 = dynamic_cast<cocos2d::ui::TextField*>(pSender);
	switch (type)
	{
	case cocos2d::ui::TextField::EventType::ATTACH_WITH_IME:
		break;
	case cocos2d::ui::TextField::EventType::DETACH_WITH_IME:
		init_friction.clear();
		this->change_input_to_array(textField3->getString(), init_friction);
		log("Friction: %d", init_friction.size());
		break;
	case cocos2d::ui::TextField::EventType::INSERT_TEXT:
		break;
	case cocos2d::ui::TextField::EventType::DELETE_BACKWARD:
		break;
	default:
		break;
	}
}


void ToolLayer::InputFxEvent(Ref *pSender, cocos2d::ui::TextField::EventType type){
	cocos2d::ui::TextField* textField4 = dynamic_cast<cocos2d::ui::TextField*>(pSender);
	switch (type)
	{
	case cocos2d::ui::TextField::EventType::ATTACH_WITH_IME:
		break;
	case cocos2d::ui::TextField::EventType::DETACH_WITH_IME:
		init_f_x.clear();
		this->change_input_to_array(textField4->getString(), init_f_x);
		break;
	case cocos2d::ui::TextField::EventType::INSERT_TEXT:
		break;
	case cocos2d::ui::TextField::EventType::DELETE_BACKWARD:
		break;
	default:
		break;
	}
}

void ToolLayer::InputFyEvent(Ref *pSender, cocos2d::ui::TextField::EventType type){
	cocos2d::ui::TextField* textField5 = dynamic_cast<cocos2d::ui::TextField*>(pSender);
	switch (type)
	{
	case cocos2d::ui::TextField::EventType::ATTACH_WITH_IME:
		break;
	case cocos2d::ui::TextField::EventType::DETACH_WITH_IME:
		init_f_y.clear();
		this->change_input_to_array(textField5->getString(), init_f_y);
		break;
	case cocos2d::ui::TextField::EventType::INSERT_TEXT:
		break;
	case cocos2d::ui::TextField::EventType::DELETE_BACKWARD:
		break;
	default:
		break;
	}
}

void ToolLayer::change_input_to_array(std::string v_x, vector<double> &output_vector){
	vector<string> v_x_array;
	v_x_array = this->split_string(v_x, ';');
	for (int i = 0; i < v_x_array.size(); i++){
		output_vector.push_back(stringToNum<double>(v_x_array.at(i)));
	}
}

vector<string> ToolLayer::split_string(string strtem, char a){
	vector<string> strvec;
	string::size_type pos1, pos2;
	pos2 = strtem.find(a);
	pos1 = 0;
	while (string::npos != pos2)
	{
		strvec.push_back(strtem.substr(pos1, pos2 - pos1));
		pos1 = pos2 + 1;
		pos2 = strtem.find(a, pos1);
	}
	strvec.push_back(strtem.substr(pos1));
	return strvec;
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
	_gamekeyboardListener = EventListenerKeyboard::create();
	_gamekeyboardListener->onKeyPressed = [&](EventKeyboard::KeyCode keyCode, Event * event){
		if (EventKeyboard::KeyCode::KEY_F == keyCode)
		{
			this->freePhysicsWorld();
		}
		else if (EventKeyboard::KeyCode::KEY_D == keyCode){
			this->nofreePhysicsWorld();
		}
		else if (EventKeyboard::KeyCode::KEY_G == keyCode){
			auto gravity = this->getScene()->getPhysicsWorld()->getGravity();
			if (gravity == Vec2(0, 0)){
				this->getScene()->getPhysicsWorld()->setGravity(GRAVITY);
			}
			else{
				this->getScene()->getPhysicsWorld()->setGravity(Vec2(0, 0));
			}
		}
	};
	_eventDispatcher->addEventListenerWithSceneGraphPriority(_gamekeyboardListener, this);
	this->schedule(schedule_selector(GameLayer::updateVelocityText), 0.04);
	this->getScene()->getPhysicsWorld()->setGravity(GRAVITY);
	_postCmdHandlers.makeJoints(this->getScene()->getPhysicsWorld(), jointsList, _genSpriteResultMap);
}

void GameLayer::initVelocityForPhysicsBody(){
	int index = 0;
	for (auto i = _genSpriteResultMap.begin(); i != _genSpriteResultMap.end(); i++)
	{
		auto sprite = i->second;
		auto cur = sprite->getPhysicsBody();
		Vec2 point;
		if (cur->isDynamic()) {
			int x_size = init_v_x.size();
			int y_size = init_v_y.size();
			log("x_size: %d, y_size:%d", x_size, y_size);
			if (x_size > 0 && y_size > 0){
				if (index < x_size && index < y_size){
					point = Vec2(init_v_x.at(index), 0 - init_v_y.at(index));
				}
				else if (index <x_size && index >= y_size){
					point = Vec2(init_v_x.at(index), 0 - init_v_y.at(y_size - 1));
				}
				else if (index >= x_size && index < y_size){
					point = Vec2(init_v_x.at(x_size - 1), 0 - init_v_y.at(index));
				}
				else{
					 point = Vec2(init_v_x.at(x_size - 1), 0 - init_v_y.at(y_size - 1));
				}
			}
			else if (x_size == 0 && y_size > 0){
				if (index < y_size){
					point = Vec2(0, 0 - init_v_y.at(index));
				}
				else{
					point = Vec2(0, 0 - init_v_y.at(y_size - 1));
				}
			}
			else if (y_size == 0 && x_size > 0){
				if (index < x_size){
					point = Vec2(init_v_x.at(index), 0);
				}
				else{
					point = Vec2(init_v_x.at(x_size - 1), 0);
				}
			}
			else {
				point = Vec2(0, 0);
			}
			
			point.x = point.x * 10;
			point.y = point.y * 10;
			cur->setVelocity(Vec2(point));
			auto absolute_point = Vec2(ZERO_POINT_X, sqrt(pow(point.x / 10, 2) + pow(point.y / 10, 2)) + ZERO_POINT_Y);
			
			_drawVelocityLayer->startDrawLocationList.push_back(absolute_point);
			index++;
		}
	}
}

void GameLayer::initForceForPhysicsBody(){
	int index = 0;
	for (auto i = _genSpriteResultMap.begin(); i != _genSpriteResultMap.end(); i++)
	{
		auto sprite = i->second;
		auto cur = sprite->getPhysicsBody();
		Vec2 point;
		if (cur->isDynamic()) {
			int x_size = this->init_f_x.size();
			int y_size = this->init_f_y.size();
			log("x_size: %d, y_size:%d", x_size, y_size);
			if (x_size > 0 && y_size > 0){
				if (index < x_size && index < y_size){
					point = Vec2(init_f_x.at(index), 0 - init_f_y.at(index));
				}
				else if (index <x_size && index >= y_size){
					point = Vec2(init_f_x.at(index), 0 - init_f_y.at(y_size - 1));
				}
				else if (index >= x_size && index < y_size){
					point = Vec2(init_f_x.at(x_size - 1), 0 - init_f_y.at(index));
				}
				else{
					point = Vec2(init_f_x.at(x_size - 1), 0 - init_f_y.at(y_size - 1));
				}
			}
			else if (x_size == 0 && y_size > 0){
				if (index < y_size){
					point = Vec2(0, 0 - init_f_y.at(index));
				}
				else{
					point = Vec2(0, 0 - init_f_y.at(y_size - 1));
				}
				
			}
			else if (y_size == 0 && x_size > 0){
				if (index < x_size){
					point = Vec2(init_f_x.at(index), 0);
				}
				else{
					point = Vec2(init_f_x.at(x_size - 1), 0);
				}
				
			}
			else {
				point = Vec2(0, 0);
			}

			point.x = point.x * 100;
			point.y = point.y * 100;
			cur->applyForce(Vec2(point));
			index++;
		}
	}
}
void GameLayer::freePhysicsWorld(){
	log("free world");
	auto index = 0;
	auto gravity = this->getScene()->getPhysicsWorld()->getGravity();

	if (gravity != Vec2(0, 0)){
		this->getScene()->getPhysicsWorld()->setGravity(Vec2(0, 0));
		for (auto i = _genSpriteResultMap.begin(); i != _genSpriteResultMap.end(); i++)
		{
			auto sprite = i->second;
			auto cur = sprite->getPhysicsBody();
			Vec2 vec = cur->getVelocity();
			this->currentLocationMap[index] = vec;
			if (cur->isDynamic()){
				cur->setVelocity(Vec2(0, 0));
				cur->setAngularVelocity(0);
				cur->resetForces();
				index++;
			}
		}
		this->begin_free_time = clock();
	}
	
}

void GameLayer::nofreePhysicsWorld() {
	log("unfree world");
	auto gravity = this->getScene()->getPhysicsWorld()->getGravity();
	if (gravity == Vec2(0, 0)){
		this->getScene()->getPhysicsWorld()->setGravity(GRAVITY);
		int index = 0;
		for (auto i = _genSpriteResultMap.begin(); i != _genSpriteResultMap.end(); i++)
		{
			auto sprite = i->second;
			auto cur = sprite->getPhysicsBody();
			Vec2 vec = this->currentLocationMap.find(index)->second;
			if (cur->isDynamic()){
				cur->setVelocity(vec);
				index++;
			}
		}
		initForceForPhysicsBody();
		this->end_free_time = clock();
		this->freeze_time = (double)(this->end_free_time - this->begin_free_time) / CLOCKS_PER_SEC;
		this->_drawVelocityLayer->freeze_time += this->freeze_time;
		log("game free time:%f", this->freeze_time);
	}
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
	auto a = contact.getShapeA()->getBody();
	auto b = contact.getShapeB()->getBody();
	if (init_friction.size() > 0){
		double friction = init_friction[0];
		log("raw friction:%f", friction);
		contact.getShapeA()->setFriction(friction);
		contact.getShapeB()->setFriction(friction);
	}
	else{
		contact.getShapeA()->setFriction(0);
		contact.getShapeB()->setFriction(0);
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
	this->InitLineColorMap();
	Size visibleSize = Director::getInstance()->getVisibleSize();
	Vec2 origin = Director::getInstance()->getVisibleOrigin();
	/**
	// input v_x
	Vec2 raw_size = Vec2(67.5, 63.5);
	_VxLabel = Label::create(TOOL_HINT_WELCOME, DEFAULT_FONT, 24, Size::ZERO, TextHAlignment::LEFT, TextVAlignment::CENTER);
	_VxLabel->setPosition(Vec2(origin.x + raw_size.y / 2 + 4 * GAP,
		origin.y + visibleSize.height - raw_size.x / 4 - GAP));
	_VxLabel->setVisible(true);
	this->addChild(_VxLabel);

	// input v_y
	_VyLabel = Label::create(TOOL_HINT_WELCOME, DEFAULT_FONT, 24, Size::ZERO, TextHAlignment::LEFT, TextVAlignment::CENTER);
	_VyLabel->setPosition(Vec2(origin.x + raw_size.y / 2 + 10 * GAP,
		origin.y + visibleSize.height - raw_size.x / 4 - GAP));
	_VyLabel->setVisible(true);
	this->addChild(_VyLabel);

	*/
	_VLabel = Label::create(TOOL_HINT_WELCOME, DEFAULT_FONT, 24, Size::ZERO, TextHAlignment::LEFT, TextVAlignment::CENTER);
	_VLabel->setPosition(Vec2(ZERO_POINT_X + 60, 300));
	_VLabel->setVisible(true);
	_VLabel->setString("0 (m/s)");
	this->addChild(_VLabel);

	_TLabel = Label::create(TOOL_HINT_WELCOME, DEFAULT_FONT, 24, Size::ZERO, TextHAlignment::LEFT, TextVAlignment::CENTER);
	_TLabel->setPosition(Vec2(300, ZERO_POINT_Y));
	_TLabel->setVisible(true);
	_TLabel->setString("0 (/s)");
	this->addChild(_TLabel);

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
	t = t - this->freeze_time;
	auto absolute_velocity = sqrt(pow(velocity.x, 2) + pow(velocity.y, 2));
	auto value = _startDrawLineMap.find(index);
	cocos2d::Color4F lineColor;
	auto gravity = this->getScene()->getPhysicsWorld()->getGravity();
	if (gravity == Vec2(0, 0)){

	}
	else{
		if (index < this->colorTypeNum) {
			lineColor = this->lineColorMap.find(index)->second;
		}
		else{
			auto color_index = index % this->colorTypeNum;
			lineColor = this->lineColorMap.find(color_index)->second;
		}
		Vec2 currentLocation = Vec2(t * 10 + ZERO_POINT_X, absolute_velocity / 10 + ZERO_POINT_Y);
		auto label = DoubleToString(t) + " (/s)";
		this->_TLabel->setString(label);
		if (value != _startDrawLineMap.end()){
			auto startDrawLineLocation = _startDrawLineMap.find(index)->second;
			currentDrawLine->drawLine(startDrawLineLocation, currentLocation, lineColor);
			_startDrawLineMap.erase(value);
		}
		else{
			int length = this->startDrawLocationList.size();
			if (index < length)
				currentDrawLine->drawLine(startDrawLocationList[index], currentLocation, lineColor);
			else{
				currentDrawLine->drawLine(startDrawLocationList[length - 1], currentLocation, currentDrawLine->getBrushColor());
			}
		}
		_startDrawLineMap[index] = currentLocation;
		updateVLabel();
		log("absolute_velocity:%f, time: %f", absolute_velocity / 10, t);
	}
	
	//log("start location:%f,%f ", _startDrawLineLocation.x, _startDrawLineLocation.y);
}

void DrawVelocityLayer::updateVLabel(){
	int length = _startDrawLineMap.size();
	string velocity_str = "";
	for (auto i = 0; i < length - 1; i++) {
		velocity_str += DoubleToString((double)(_startDrawLineMap.find(i)->second.y - ZERO_POINT_Y)) + "; ";
	}
	velocity_str += DoubleToString((double)(_startDrawLineMap.find(length - 1)->second.y - ZERO_POINT_Y)) + " (m / s)";

	this->_VLabel->setString(velocity_str);
	this->_VLabel->setPosition(Vec2(ZERO_POINT_X + 24 * length * 1.8, 300));
}
void DrawVelocityLayer::InitLineColorMap(){
	this->lineColorMap[0] = Color4F(1, 1, 1, 1);
	this->lineColorMap[1] = Color4F(0, 1, 0, 1);
	this->lineColorMap[2] = Color4F(0, 0, 1, 1);
	this->lineColorMap[3] = Color4F(1, 0, 0, 1);
	this->lineColorMap[4] = Color4F(0, 1, 1, 1);
	this->lineColorMap[5] = Color4F(1, 0, 1, 1);
	this->colorTypeNum = this->lineColorMap.size();
}