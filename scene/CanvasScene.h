#ifndef __CANVAS_SCENE_H__
#define __CANVAS_SCENE_H__

#include "cocos2d.h"
#include "CanvasLayer.h"
#include "DrawableSprite.h"
#include "geometry/recognizer/GeometricRecognizerNode.h"
#include "geometry/handler/CommandHandler.h"
#include "ui/CocosGUI.h"
#include "time.h"
class CanvasScene;
class GameCanvasLayer;
class GameLayer;
class ToolLayer;
class DrawVelocityLayer;

// Event
#define EVENT_RECOGNIZE_SUCCESS			"onRecognizeSuccess"
#define EVENT_RECOGNIZE_FAILED			"onRecognizeFailed"
#define EVENT_SIMULATE_START			"onSimulateStart"
#define EVENT_SIMULATE_STOP				"onSimulateStop"
#define EVENT_TOGGLE_PHYSICS_DEBUG_MODE	"onTogglePhysicsDebugMode"

#define ZERO_POINT_X 15
#define ZERO_POINT_Y 15
/**
 * Canvas/Game Scene
 * @see cocos2d::Scene
 */
class CanvasScene : public cocos2d::Scene
{
public:

	// Constructor
	CanvasScene();

	/**
	 * Call when Canvas Scene inititialized, override
	 * @see cocos2d::Node::init
	 */
	virtual bool init();

	/**
	 * Call when Canvas Scene enters the 'stage', override
	 * @see cocos2d::Node::onEnter
	 */
	virtual void onEnter();

	/**
	 * Call when Canvas Scene exits the 'stage', override
	 * @see cocos2d::Node::onEnter
	 */
	virtual void onExit();

	/**
	 * Start simulating game
	 */
	void startSimulate();

	/**
 	 * Stop simulating game
	 */
	void stopSimulate();
	
	/**
	 * Toggle debug view mode
	 * @return true if debug view mode is on, false otherwise
	 */
	bool toggleDebugDraw();

	/**
	 * Implement the "static create()" method manually
	 */
	CREATE_FUNC(CanvasScene);

private:
	GameCanvasLayer*				_canvasLayer;		// a pointer to canvas layer
	ToolLayer*						_toolLayer;			// a pointer to tool layer
	GameLayer*						_gameLayer;			// a pointer to game layer

	cocos2d::EventListenerKeyboard* _keyboardListener;	// keyboard listener for user input
	
	bool							_debugDraw;			// status for debug view mode, true when debug mode is on, false otherwise 
};

/**
 * Game Canvas Layer
 * You can draw things in this layer 
 * @see CanvasLayer
 */
class GameCanvasLayer :public CanvasLayer
{
public:
	
	// Constructor
	GameCanvasLayer() :_jointMode(false){}
	
	/**
	 * Call when GameCanvasLayer inititialized, override
	 * @see cocos2d::Node::init
	 */
	virtual bool init();

	/**
	 * Call when GameCanvasLayer enters the 'stage', override
	 * @see cocos2d::Node::onEnter
	 */
	virtual void onEnter();
	
	/**
	 * Call when GameCanvasLayer exits the 'stage', override
	 * @see cocos2d::Node::onEnter
	 */
	virtual void onExit();

	/**
	 * Called whe mouse down, override
	 * @see CanvasLaye::ronMouseDown
	 */
	virtual void onMouseDown(cocos2d::EventMouse* event);

	/**
	* Called when mouse up, override
	*/
	virtual void onMouseUp(cocos2d::EventMouse* event);
	/**
	 * Recognize shape
	 */
	void recognize();

	/**
	 * Redraw Current Shape
	 */
	void redrawCurrentNode();

	/**
	 * Remove sprite if it's unrecognized
	 * @param target	a pointer to existing sprite
	 */
	void removeUnrecognizedSprite(DrawableSprite* target);

	/**
	 * Start game simulation
	 */
	void startGameSimulation();
	
	/**
	 * Stop game simulation
	 */
	void stopGameSimulation();

	/**
	 * Switch to wew draw node
	 * @return a pointer to new draw node
	 * @see DrawableSprite
	 */
	DrawableSprite* switchToNewDrawNode();
	
	/**
	 * Create game simulation layer
	 * @return a pointer to GameLayer instance, auto release object
	 */
	GameLayer* createGameLayer();
	
	/**
	 * Implement the "static create()" method manually
	 */
	CREATE_FUNC(GameCanvasLayer);

private:
	bool						_jointMode;			// status for joint draw mode, true joint draw mode is on, false otherwise

	std::list<DrawableSprite*>	_drawNodeList;		// current drawn nodes 
	DrawSpriteResultMap			_drawNodeResultMap;	// DrawableSprite-RecognizedSprite map
	GeometricRecognizerNode*	_geoRecognizer;		// a pointer to GeometricRecognizerNode
	PreCommandHandlerFactory	_preCmdHandlers;	// pre-command handlers

};

/**
 * Tool Layer
 * A container for tool hints, buttons
 * @see CanvasLayer
 */
class ToolLayer : public cocos2d::Layer
{
public:

	/**
	 * Call when ToolLayer inititialized, override
	 * @see cocos2d::Node::init
	 */
	virtual bool init();

	/**
	 * Call when ToolLayer enters the 'stage', override
	 * @see cocos2d::Node::onEnter
	 */
	virtual void onEnter();
	
	/**
	 * Call when ToolLayer exits the 'stage', override
	 * @see cocos2d::Node::onEnter
	 */
	virtual void onExit();

	/**
	 * Implement the "static create()" method manually
	 */
	CREATE_FUNC(ToolLayer);

	/**
	 * Event callbacks for startSimulate
	 * @param pSender	event caller
	 */
	void startSimulateCallback(cocos2d::Ref* pSender);

	/**
	 * Event callbacks for stopSimulate
	 * @param pSender	event caller
	 */
	void stopSimulateCallback(cocos2d::Ref* pSender);

	/**
	 * Event callbacks for startJointMode, not used now
	 * @param pSender	event caller
	 */
	void startJointModeCallback(cocos2d::Ref* pSender);

	/**
	 * Event callbacks for stopJointMode, not used now
	 * @param pSender	event caller
	 */
	void stopJointModeCallback(cocos2d::Ref* pSender);

	/**
	 * Toggle debug mode
	 * @param isDebug	debug status
	 */
	void toggleDebugMode(bool isDebug);

	/**
	 * Set parent scene
	 * @param canvasScene	target parent scene
	 */
	void setParentScene(CanvasScene* canvasScene)
	{
		this->_canvasScene = canvasScene;
	}

	/**
	 * Called when sprite is recognized successfully
	 * @param event	event argument
	 * @see cocos2d::EventCustom
	 */
	void onRecognizeSuccess(cocos2d::EventCustom* event);

	/**
	 * Called when sprite is failed to be recognized
	 * @param event	event argument
	 * @see cocos2d::EventCustom
	 */
	void onRecognizeFailed(cocos2d::EventCustom* event);

	void InputVxEvent(Ref *pSender, cocos2d::ui::TextField::EventType type);

	void InputVyEvent(Ref *pSender, cocos2d::ui::TextField::EventType type);

	void InputFrictionEvent(Ref *pSender, cocos2d::ui::TextField::EventType type);

	vector<string> split_string(string strtem, char a);

	void change_input_to_array(std::string v_x, vector<double> &output_vector);

	std::vector<double> init_v_x;
	std::vector<double> init_v_y;
	std::vector<double> init_friction;



private:
	cocos2d::EventListenerCustom*	_recognizeSuccessListener;	// recognize success listener
	cocos2d::EventListenerCustom*	_recognizeFailedListener;	// recognize failed listener

	cocos2d::Sprite*				_spriteAssistant;			// assistant sprite
	cocos2d::Label*					_labelHint;					// tool hint label
	cocos2d::MenuItemImage*			_menuStartSimulate;			// start simulate menu
	cocos2d::MenuItemImage*			_menuStopSimulate;			// stop simulate menu
	cocos2d::MenuItemImage*			_menuStartJoint;			// start joint mode menu, not used now
	cocos2d::MenuItemImage*			_menuStopJoint;				// start joint mode menu, not used now
	cocos2d::MenuItemImage*			_menuStartDebug;			// start debug mode menu
	
	CanvasScene*					_canvasScene;				// parent scene	
	cocos2d::ui::TextField* _VxField;
	cocos2d::ui::TextField* _VyField;
	cocos2d::ui::TextField* _FrictionField;

};


/**
 * Game Layer
 * Simulating physics effect
 * @see cocos2d::Layer
 */
class GameLayer : public cocos2d::Layer
{
public:

	/**
	 * Constructor
	 * @param	current drawn nodes 
	 * @param	DrawableSprite-RecognizedSprite map
	 * @see DrawableSprite
	 * @see DrawSpriteResultMap
	 */
	GameLayer(std::list<DrawableSprite*>&, DrawSpriteResultMap&);
	/**
	 * Call when ToolLayer inititialized, override
	 * @see cocos2d::Node::init
	 */
	virtual bool init();
	
	/**
	 * Call when GameLayer enters the 'stage', override
	 * @see cocos2d::Node::onEnter
	 */
	virtual void onEnter();
	
	/**
	 * Call when GameLayer exits the 'stage', override
	 * @see cocos2d::Node::onEnter
	 */
	virtual void onExit();

	//void onAcceleration(Acceleration* acc, Event* event);

	bool onPhysicsContactBegin(const cocos2d::PhysicsContact& contact);

	/**
	 * Implement the "static create" method manually with non-empty parameters
	 */
	static GameLayer *GameLayer::create(list<DrawableSprite*>&, DrawSpriteResultMap&, cocos2d::Scene*);

	void recordVelocityCallBack(cocos2d::Ref* pSender);

	void updateVelocityText(float t);

	std::vector<double> init_v_x;
	std::vector<double> init_v_y;
	std::vector<double> init_friction;


	void initVelocityForPhysicsBody();
	clock_t  _begin_move;
private:
	std::list<DrawableSprite*>& _drawNodeList;			// current drawn nodes 
	DrawSpriteResultMap&		_drawNodeResultMap;		// DrawableSprite-RecognizedSprite map
	GenSpriteResultMap			_genSpriteResultMap;	// DrawableSprite-Sprite map, generated sprites, with physics body
	PostCommandHandlerFactory	_postCmdHandlers;		// post-command handlers
	DrawVelocityLayer           * _drawVelocityLayer;
	
};


class DrawVelocityLayer : public CanvasLayer
{
public:
	DrawVelocityLayer(){

	};
	/**
	* Call when DrawVelocityLayer inititialized, override
	* @see cocos2d::Node::init
	*/
	virtual bool init();

	/**
	* Call when DrawVelocityLayer enters the 'stage', override
	* @see cocos2d::Node::onEnter
	*/
	virtual void onEnter();

	/**
	* Call when DrawVelocityLayer exits the 'stage', override
	* @see cocos2d::Node::onEnter
	*/
	virtual void onExit();

	DrawVelocityLayer *createVelocityLayer();

	void drawVelocityLine(cocos2d::Vec2 velocity, double t, int index);

	CREATE_FUNC(DrawVelocityLayer);

	std::vector<cocos2d::Vec2> startDrawLocationList;

private:
	double t;
	bool isDrawingLine;
	cocos2d::Vec2 velocity;
	cocos2d::ui::ImageView *_gestureBackgroundView;
	DrawableSprite *currentDrawLine;
	cocos2d::Vec2 _startDrawLineLocation;
	std::map<int, cocos2d::Vec2> _startDrawLineMap;
};

template <class Type>
Type stringToNum(const string& str)
{
	istringstream iss(str);
	Type num;
	iss >> num;
	return num;
}

#endif // __CANVAS_SCENE_H__