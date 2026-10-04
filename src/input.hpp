//! @file input.hpp

#pragma once

#include "main.hpp"

#include <string>
#include <unordered_map>
#include <vector>

class GameController;
using BindingPair = std::pair<const char*, const char*>;

static constexpr BindingPair gamepadDefaults[] = {
#ifdef NINTENDO_DEBUG
    {"ConsoleCommand1", "ButtonLeftBumper"},
    {"ConsoleCommand2", "ButtonRightBumper"},
    {"ConsoleCommand3", "ButtonBack"},
#endif

    {"MenuUp", "DpadY-"},
    {"MenuLeft", "DpadX-"},
    {"MenuRight", "DpadX+"},
    {"MenuDown", "DpadY+"},
    {"MenuConfirm", "ButtonA"},
    {"MenuCancel", "ButtonB"},
    {"MenuListCancel", "ButtonB"},

#ifdef NINTENDO
    {"MenuAlt1", "ButtonY"},
    {"MenuAlt2", "ButtonX"},
#else
    {"MenuAlt1", "ButtonX"},
    {"MenuAlt2", "ButtonY"},
#endif

    {"MenuStart", "ButtonStart"},
    {"MenuSelect", "ButtonBack"},
    {"MenuPageLeft", "ButtonLeftBumper"},
    {"MenuPageRight", "ButtonRightBumper"},
    {"MenuPageLeftAlt", "LeftTrigger"},
    {"MenuPageRightAlt", "RightTrigger"},

    {"AltMenuUp", "StickLeftY-"},
    {"AltMenuLeft", "StickLeftX-"},
    {"AltMenuRight", "StickLeftX+"},
    {"AltMenuDown", "StickLeftY+"},

    {"MenuScrollUp", "StickRightY-"},
    {"MenuScrollLeft", "StickRightX-"},
    {"MenuScrollRight", "StickRightX+"},
    {"MenuScrollDown", "StickRightY+"},

    {"HotbarFacebarModifierLeft", "ButtonLeftBumper"},
    {"HotbarFacebarModifierRight", "ButtonRightBumper"},

    {"InventoryMoveUp", "DpadY-"},
    {"InventoryMoveLeft", "DpadX-"},
    {"InventoryMoveRight", "DpadX+"},
    {"InventoryMoveDown", "DpadY+"},

    {"InventoryMoveUpAnalog", "StickRightY-"},
    {"InventoryMoveLeftAnalog", "StickRightX-"},
    {"InventoryMoveRightAnalog", "StickRightX+"},
    {"InventoryMoveDownAnalog", "StickRightY+"},

    {"InventoryCharacterRotateLeft", "StickRightX-"},
    {"InventoryCharacterRotateRight", "StickRightX+"},
    {"InventoryTooltipPromptAppraise", "ButtonLeftStick"},
    {"Expand Inventory Tooltip", "ButtonRightStick"},

    {"UINavLeftBumper", "ButtonLeftBumper"},
    {"UINavRightBumper", "ButtonRightBumper"},
    {"UINavLeftTrigger", "LeftTrigger"},
    {"UINavRightTrigger", "RightTrigger"},

    {"Move Forward", "StickLeftY-"},
    {"Move Left", "StickLeftX-"},
    {"Move Backward", "StickLeftY+"},
    {"Move Right", "StickLeftX+"},

    {"Turn Left", "StickRightX-"},
    {"Turn Right", "StickRightX+"},
    {"Look Up", "StickRightY-"},
    {"Look Down", "StickRightY+"},

    {"PaperDollContextMenu", "ButtonLeftStick"},

    {"LogHome", "ButtonLeftStick"},
    {"LogEnd", "ButtonRightStick"},
    {"LogPageDown", "ButtonRightBumper"},
    {"LogPageUp", "ButtonLeftBumper"},
    {"LogScrollDown", "StickRightY+"},
    {"LogClose", "ButtonB"},
    {"LogScrollUp", "StickRightY-"},

    {"MinimapPing", "ButtonA"},
    {"MinimapClose", "ButtonB"},
    {"MinimapRight", "StickRightX+"},
    {"MinimapLeft", "StickRightX-"},
    {"MinimapDown", "StickRightY+"},
    {"MinimapUp", "StickRightY-"},

    {"ResetPortraitRotation", "ButtonRightStick"},
    {"GamepadLoginA", "ButtonA"},

#ifndef NINTENDO
    {"GamepadLoginB", "ButtonB"},
#endif

    {"GamepadLoginStart", "ButtonStart"}
};
static constexpr BindingPair keyboardDefaults[] = {
    {"GamepadScreenshot", "F6"},

    {"MenuMouseWheelUp", "MouseWheelUp"},
    {"MenuMouseWheelDown", "MouseWheelDown"},
    {"MenuMouseWheelUpAlt", "MouseWheelUp"},
    {"MenuMouseWheelDownAlt", "MouseWheelDown"},

    {"InventoryCharacterRotateLeftMouse", "MouseWheelUp"},
    {"InventoryCharacterRotateRightMouse", "MouseWheelDown"},

    {"MenuLeftClick", "Mouse1"},
    {"MenuMiddleClick", "Mouse2"},
    {"MenuRightClick", "Mouse3"},
    {"InspectWithMouse", "Mouse1"},

    {"MinimapPing", "Mouse1"},
    {"ResetPortraitRotation", "Mouse2"},

    {"KeyboardLogin", "Space"},

    {"LogHome", "Home"},
    {"LogEnd", "End"},
    {"LogPageDown", "PageDown"},
    {"LogPageUp", "PageUp"},
    {"LogScrollDown", "MouseWheelDown"},
    {"LogScrollUp", "MouseWheelUp"},

    {"MenuUp", "Up"},
    {"MenuLeft", "Left"},
    {"MenuRight", "Right"},
    {"MenuDown", "Down"},
    {"MenuConfirm", "Space"},
    {"MenuCancel", "Escape"},
    {"MenuListCancel", "Escape"},

    {"MenuStart", "Return"},
    {"MenuSelect", "Backspace"},
    {"MenuPageLeft", "["},
    {"MenuPageRight", "]"},

    {"Console Command", "/"}
};
static const std::unordered_map<const char*, BindingPair> NintendoSwitchMappings = {
    {"Mouse1", {"Mouse/Mouse_LClick_Pressed_00.png", "Mouse/Mouse_LClick_Unpressed_00.png"}},
    {"Mouse2", {"Mouse/Mouse_MClick_Pressed_00.png", "Mouse/Mouse_MClick_Unpressed_00.png"}},
    {"Mouse3", {"Mouse/Mouse_RClick_Pressed_00.png", "Mouse/Mouse_RClick_Unpressed_00.png"}},
    {"Mouse4", {"Mouse/Mouse_M4_Pressed_00.png", "Mouse/Mouse_M4_Unpressed_00.png"}},
    {"Mouse5", {"Mouse/Mouse_M5_Pressed_00.png", "Mouse/Mouse_M5_Unpressed_00.png"}},
    {"MouseWheelDown", {"Mouse/Mouse_MWheelDown_Pressed_00.png", "Mouse/Mouse_MWheelDown_Unpressed_00.png"}},
    {"MouseWheelUp", {"Mouse/Mouse_MWheelUp_Pressed_00.png", "Mouse/Mouse_MWheelUp_Unpressed_00.png"}},
    {"ButtonA", {"Button_Xbox_DarkA_00.png", "Button_Xbox_DarkA_Press_00.png"}},
    {"ButtonB", {"Button_Xbox_DarkB_00.png", "Button_Xbox_DarkB_Press_00.png"}},
    {"ButtonX", {"Button_Xbox_DarkX_00.png", "Button_Xbox_DarkX_Press_00.png"}},
    {"ButtonY", {"Button_Xbox_DarkY_00.png", "Button_Xbox_DarkY_Press_00.png"}},
    {"ButtonLeftBumper", {"G_Switch_L00.png", "G_Switch_L_Press00.png"}},
    {"ButtonRightBumper", {"G_Switch_R00.png", "G_Switch_R_Press00.png"}},
    {"ButtonLeftStick", {"Stick_Switch_L_00.png", "Stick_Switch_L_Pressed_00.png"}},
    {"ButtonRightStick", {"Stick_Switch_R_00.png", "Stick_Switch_R_Pressed_00.png"}},
    {"ButtonStart", {"PlusMed00.png", "PlusMed_Press00.png"}},
    {"ButtonBack", {"MinusMed00.png", "MinusMed_Press00.png"}},
    {"StickLeftX-", {"Stick_Switch_L_Left_00.png", "Stick_Switch_L_Left_Pressed_00.png"}},
    {"StickLeftX+", {"Stick_Switch_L_Right_00.png", "Stick_Switch_L_Right_Pressed_00.png"}},
    {"StickLeftY-", {"Stick_Switch_L_Up_00.png", "Stick_Switch_L_Up_Pressed_00.png"}},
    {"StickLeftY+", {"Stick_Switch_L_Down_00.png", "Stick_Switch_L_Down_Pressed_00.png"}},
    {"StickRightX-", {"Stick_Switch_R_Left_00.png", "Stick_Switch_R_Left_Pressed_00.png"}},
    {"StickRightX+", {"Stick_Switch_R_Right_00.png", "Stick_Switch_R_Right_Pressed_00.png"}},
    {"StickRightY-", {"Stick_Switch_R_Up_00.png", "Stick_Switch_R_Up_Pressed_00.png"}},
    {"StickRightY+", {"Stick_Switch_R_Down_00.png", "Stick_Switch_R_Down_Pressed_00.png"}},
    {"LeftTrigger", {"G_Switch_ZL00.png", "G_Switch_ZL_Press00.png"}},
    {"RightTrigger", {"G_Switch_ZR00.png", "G_Switch_ZR_Press00.png"}},
    {"DpadX-", {"G_Switch_Direct_Left_Press00.png", "G_Switch_Direct_00.png"}},
    {"DpadX+", {"G_Switch_Direct_Right_Press00.png", "G_Switch_Direct_00.png"}},
    {"DpadY-", {"G_Switch_Direct_Up_Press00.png", "G_Switch_Direct_00.png"}},
    {"DpadY+", {"G_Switch_Direct_Down_Press00.png", "G_Switch_Direct_00.png"}},
};
static const std::unordered_map<const char*, BindingPair> PlayStationMappings = {
    {"Mouse1", {"Mouse/Mouse_LClick_Pressed_00.png", "Mouse/Mouse_LClick_Unpressed_00.png"}},
    {"Mouse2", {"Mouse/Mouse_MClick_Pressed_00.png", "Mouse/Mouse_MClick_Unpressed_00.png"}},
    {"Mouse3", {"Mouse/Mouse_RClick_Pressed_00.png", "Mouse/Mouse_RClick_Unpressed_00.png"}},
    {"Mouse4", {"Mouse/Mouse_M4_Pressed_00.png", "Mouse/Mouse_M4_Unpressed_00.png"}},
    {"Mouse5", {"Mouse/Mouse_M5_Pressed_00.png", "Mouse/Mouse_M5_Unpressed_00.png"}},
    {"MouseWheelDown", {"Mouse/Mouse_MWheelDown_Pressed_00.png", "Mouse/Mouse_MWheelDown_Unpressed_00.png"}},
    {"MouseWheelUp", {"Mouse/Mouse_MWheelUp_Pressed_00.png", "Mouse/Mouse_MWheelUp_Unpressed_00.png"}},
    {"ButtonA", {"G_PS_X00.png", "G_PS_X_Press00.png"}},
    {"ButtonB", {"G_PS_O00.png", "G_PS_O_Press00.png"}},
    {"ButtonX", {"G_PS_Box00.png", "G_PS_Box_Press00.png"}},
    {"ButtonY", {"G_PS_Tri00.png", "G_PS_Tri_Press00.png"}},
    {"ButtonLeftBumper", {"Button_PS_L1_00.png", "Button_PS_L1_Press_00.png"}},
    {"ButtonRightBumper", {"Button_PS_R1_00.png", "Button_PS_R1_Press_00.png"}},
    {"ButtonLeftStick", {"Stick_PS_L_00.png", "Stick_PS_L_Pressed_00.png"}},
    {"ButtonRightStick", {"Stick_PS_R_00.png", "Stick_PS_R_Pressed_00.png"}},
    {"ButtonStart", {"Button_OptC.png", "Button_OptC_Press00.png"}},
    {"ButtonBack", {"Button_Touchpad_PS5_00B.png", "Button_Touchpad_PS5_00C.png"}},
    {"StickLeftX-", {"Stick_PS_L_Left_00.png", "Stick_PS_L_Left_Pressed_00.png"}},
    {"StickLeftX+", {"Stick_PS_L_Right_00.png", "Stick_PS_L_Right_Pressed_00.png"}},
    {"StickLeftY-", {"Stick_PS_L_Up_00.png", "Stick_PS_L_Up_Pressed_00.png"}},
    {"StickLeftY+", {"Stick_PS_L_Down_00.png", "Stick_PS_L_Down_Pressed_00.png"}},
    {"StickRightX-", {"Stick_PS_R_Left_00.png", "Stick_PS_R_Left_Pressed_00.png"}},
    {"StickRightX+", {"Stick_PS_R_Right_00.png", "Stick_PS_R_Right_Pressed_00.png"}},
    {"StickRightY-", {"Stick_PS_R_Up_00.png", "Stick_PS_R_Up_Pressed_00.png"}},
    {"StickRightY+", {"Stick_PS_R_Down_00.png", "Stick_PS_R_Down_Pressed_00.png"}},
    {"LeftTrigger", {"Button_PS_L2_00.png", "Button_PS_L2_Press_00.png"}},
    {"RightTrigger", {"Button_PS_R2_00.png", "Button_PS_R2_Press_00.png"}},
    {"DpadX-", {"G_Direct_Left_Press00.png", "G_Direct_00.png"}},
    {"DpadX+", {"G_Direct_Right_Press00.png", "G_Direct_00.png"}},
    {"DpadY-", {"G_Direct_Up_Press00.png", "G_Direct_00.png"}},
    {"DpadY+", {"G_Direct_Down_Press00.png", "G_Direct_00.png"}},
};
static const std::unordered_map<const char*, BindingPair> XboxMappings = {
    {"Mouse1", {"Mouse/Mouse_LClick_Pressed_00.png", "Mouse/Mouse_LClick_Unpressed_00.png"}},
    {"Mouse2", {"Mouse/Mouse_MClick_Pressed_00.png", "Mouse/Mouse_MClick_Unpressed_00.png"}},
    {"Mouse3", {"Mouse/Mouse_RClick_Pressed_00.png", "Mouse/Mouse_RClick_Unpressed_00.png"}},
    {"Mouse4", {"Mouse/Mouse_M4_Pressed_00.png", "Mouse/Mouse_M4_Unpressed_00.png"}},
    {"Mouse5", {"Mouse/Mouse_M5_Pressed_00.png", "Mouse/Mouse_M5_Unpressed_00.png"}},
    {"MouseWheelDown", {"Mouse/Mouse_MWheelDown_Pressed_00.png", "Mouse/Mouse_MWheelDown_Unpressed_00.png"}},
    {"MouseWheelUp", {"Mouse/Mouse_MWheelUp_Pressed_00.png", "Mouse/Mouse_MWheelUp_Unpressed_00.png"}},
    {"ButtonA", {"Button_Xbox_DarkA_00.png", "Button_Xbox_DarkA_Press_00.png"}},
    {"ButtonB", {"Button_Xbox_DarkB_00.png", "Button_Xbox_DarkB_Press_00.png"}},
    {"ButtonX", {"Button_Xbox_DarkX_00.png", "Button_Xbox_DarkX_Press_00.png"}},
    {"ButtonY", {"Button_Xbox_DarkY_00.png", "Button_Xbox_DarkY_Press_00.png"}},
    {"ButtonLeftBumper", {"Button_Xbox_LB_00.png", "Button_Xbox_LB_Press_00.png"}},
    {"ButtonRightBumper", {"Button_Xbox_RB_00.png", "Button_Xbox_RB_Press_00.png"}},
    {"ButtonLeftStick", {"Stick_Xbox_L_00.png", "Stick_Xbox_L_Pressed_00.png"}},
    {"ButtonRightStick", {"Stick_Xbox_R_00.png", "Stick_Xbox_R_Pressed_00.png"}},
    {"ButtonStart", {"Button_Xbox_Menu_00.png", "Button_Xbox_Menu_Press_00.png"}},
    {"ButtonBack", {"Button_Xbox_View_00.png", "Button_Xbox_View_Press_00.png"}},
    {"StickLeftX-", {"Stick_Xbox_L_Left_00.png", "Stick_Xbox_L_Left_Pressed_00.png"}},
    {"StickLeftX+", {"Stick_Xbox_L_Right_00.png", "Stick_Xbox_L_Right_Pressed_00.png"}},
    {"StickLeftY-", {"Stick_Xbox_L_Up_00.png", "Stick_Xbox_L_Up_Pressed_00.png"}},
    {"StickLeftY+", {"Stick_Xbox_L_Down_00.png", "Stick_Xbox_L_Down_Pressed_00.png"}},
    {"StickRightX-", {"Stick_Xbox_R_Left_00.png", "Stick_Xbox_R_Left_Pressed_00.png"}},
    {"StickRightX+", {"Stick_Xbox_R_Right_00.png", "Stick_Xbox_R_Right_Pressed_00.png"}},
    {"StickRightY-", {"Stick_Xbox_R_Up_00.png", "Stick_Xbox_R_Up_Pressed_00.png"}},
    {"StickRightY+", {"Stick_Xbox_R_Down_00.png", "Stick_Xbox_R_Down_Pressed_00.png"}},
    {"LeftTrigger", {"Button_Xbox_LT_00.png", "Button_Xbox_LT_Press_00.png"}},
    {"RightTrigger", {"Button_Xbox_RT_00.png", "Button_Xbox_RT_Press_00.png"}},
    {"DpadX-", {"G_Direct_Left_Press00.png", "G_Direct_00.png"}},
    {"DpadX+", {"G_Direct_Right_Press00.png", "G_Direct_00.png"}},
    {"DpadY-", {"G_Direct_Up_Press00.png", "G_Direct_00.png"}},
    {"DpadY+", {"G_Direct_Down_Press00.png", "G_Direct_00.png"}},
};

//! The Input class provides a way to bind physical keys to abstract names like "Move Forward",
//! collect the input data from the physical devices, and provide it back to you for any purpose.
class Input
{
public:
    Input() = default;

    Input(const Input&) = delete;
    Input& operator=(const Input&) = delete;

    Input(Input&&) noexcept = default;
    Input& operator=(Input&&) noexcept = default;

    static std::vector<Input> inputs;
    int player = 0;

    //! set default bindings for all players
    static void defaultBindings();

    //! input mapping
    struct binding_t
    {
        std::string input;
        float analog = 0.f; // input power [0.0 - 1.0]
        bool binary = false; // input is "active" (ie button is pressed or not)
        bool consumed = false; // input is "consumed" (ie it has been used for an action)
        Uint32 heldTicks = 0; // tick when the binding was first activated

        //! bind type
        enum bindtype_t
        {
            INVALID,
            KEYBOARD,
            CONTROLLER_AXIS,
            CONTROLLER_BUTTON,
            MOUSE_BUTTON,
            JOYSTICK_AXIS,
            JOYSTICK_BUTTON,
            JOYSTICK_HAT,
            //JOYSTICK_BALL,
            NUM
        };

        bindtype_t type = INVALID;

        //! keyboard binding info
        SDL_Keycode keycode = SDLK_UNKNOWN;

        //! gamepad binding info
        int padIndex = -1;
        SDL_GameController* pad = nullptr;
        SDL_GameControllerAxis padAxis = SDL_CONTROLLER_AXIS_INVALID;
        SDL_GameControllerButton padButton = SDL_CONTROLLER_BUTTON_INVALID;
        bool padAxisNegative = false;

        //! joystick binding info
        SDL_Joystick* joystick = nullptr;
        int joystickAxis = 0;
        bool joystickAxisNegative = false;
        int joystickButton = 0;
        int joystickHat = 0;
        Uint8 joystickHatState = 0;

        //! mouse button info
        int mouseButton = 0;

        //! checks type is a gamepad-adjacent input (i.e not keyboard or mouse)
        [[nodiscard]] bool isBindingUsingGamepad() const { return (type != KEYBOARD && type != MOUSE_BUTTON && type != INVALID); }
        [[nodiscard]] bool isBindingUsingKeyboard() const { return (type == KEYBOARD || type == MOUSE_BUTTON); }
    };

    //! useful way to get direct access to bindings
    auto& getBindings() const { return bindings; }

    //! useful way to get direct access to more bindings
    auto& getKeyboardBindings() { return kb_bindings; }
    auto& getSystemKeyboardBindings() { return kb_system_bindings; }
    auto& getGamepadBindings() { return gamepad_bindings; }
    auto& getSystemGamepadBindings() { return gamepad_system_bindings; }
    auto& getJoystickBindings() { return joystick_bindings; }
    auto& getSystemJoystickBindings() { return joystick_system_bindings; }
    void setKeyboardBindings(const std::unordered_map<std::string, std::string>& toSet) { kb_bindings = toSet; }
    void setGamepadBindings(const std::unordered_map<std::string, std::string>& toSet) { gamepad_bindings = toSet; }
    void setJoystickBindings(const std::unordered_map<std::string, std::string>& toSet) { joystick_bindings = toSet; }

    //! controller type
    enum playerControlType_t
    {
        PLAYER_CONTROLLED_BY_INVALID,
        PLAYER_CONTROLLED_BY_KEYBOARD,
        PLAYER_CONTROLLED_BY_CONTROLLER,
        PLAYER_CONTROLLED_BY_JOYSTICK,
        NUM
    };

    playerControlType_t getPlayerControlType();

    //! disable all bindings temporarily
    void setDisabled(bool _disabled) { disabled = _disabled; }

    //! get the status of disabled variable
    auto isDisabled() const { return disabled; }

    //! gets the analog value of a particular input binding
    //! @param binding the binding to query
    //! @return the analog value (range = -1.f : +1.f)
    float analog(const char* binding) const;

    //! gets the binary value of a particular input binding
    //! @param binding the binding to query
    //! @return the bool value (false = not pressed, true = pressed)
    bool binary(const char* binding) const;

    //! gets the binary value of a particular input binding, if it's not been consumed
    //! releasing the input and retriggering it "unconsumes"
    //! @param binding the binding to query
    //! @return the bool value (false = not pressed, true = pressed)
    bool binaryToggle(const char* binding) const;

    //! consume an input action no matter what
    //! @param binding the binding to be consumed
    //! @return true if the toggle was consumed (ie the button was pressed)
    bool consumeBinary(const char* binding);

    //! consume an input action, if it is being pressed
    //! @param binding the binding to be consumed
    //! @return true if the toggle was consumed (ie the button was pressed)
    bool consumeBinaryToggle(const char* binding);

    //! gets the 'held' button value of a particular input binding, if it's not been consumed
    //! requires the button to be held for 'BUTTON_HELD_TICKS' to return true
    //! @param binding the binding to query
    //! @return the bool value (false = not pressed, true = pressed for greater than tick time)
    bool binaryHeldToggle(const char* binding) const;

    //! gets the input mapped to a particular input binding
    //! @param binding the binding to query
    //! @return the input mapped to the given binding
    const char* binding(const char* binding) const;

    //! bind the given action to the given input
    //! @param name the action to bind
    //! @param input the input to bind to the action
    void bind(const char* binding, const char* input);

    //! refresh bindings (eg after a new controller is detected)
    void refresh();

    //! updates the state of all current bindings from the physical devices
    void update();

    //! updates the state of release consumed variable on bindings (needs to happen exactly once per game tick)
    void updateReleaseConsumed();

    //! if true, Y axis for mouse/gamepads/joysticks is inverted
    bool inverted = false;

    //! return the binding_t struct for the input name
    binding_t input(const char* binding) const;

    enum class ControllerType
    {
        PlayStation,
        NintendoSwitch,
        Xbox,
        SteamDeck,
    };

    static ControllerType getControllerType(int index);
    static const char* getKeyboardGlyph();
    static const char* getControllerGlyph(int index);
    ControllerType getControllerType() const;
    const char* getControllerGlyph() const;

    static std::string getGlyphPathForInput(const char* input, bool pressed = false,
                                            ControllerType type = ControllerType::Xbox);
    static std::string getGlyphPathForBinding(const binding_t& binding, bool pressed = false);
    std::string getGlyphPathForBinding(const char* binding, bool pressed = false) const;

    static float getJoystickRebindingDeadzone() { return rebinding_deadzone; }
    static float getAnalogToggleThreshold() { return analogToggleThreshold; }

    //! list of connected input devices
    static std::string lastInputOfAnyKind;
    static int waitingToBindControllerForPlayer;
    static std::unordered_map<int, SDL_GameController*> gameControllers;
    static std::unordered_map<int, SDL_Joystick*> joysticks;
    static std::unordered_map<SDL_Keycode, bool> keys;
    static bool mouseButtons[18];
    static const int MOUSE_WHEEL_UP;
    static const int MOUSE_WHEEL_DOWN;

    //! consume inputs related to the players face-hotbar if it is open
    void consumeBindingsSharedWithFaceHotbar();

    //! consume bindings that all use the same input as given binding
    void consumeBindingsSharedWithBinding(const char* binding);

    //! return true if binding conflicts with system binding (i.e left/right click, scroll wheel)
    bool bindingIsSharedWithKeyboardSystemBinding(const char* binding);

    //! get list of bindings for given input
    std::vector<std::string> getBindingsForInput(const char* input) const;

private:
    std::unordered_map<std::string, binding_t> bindings;

    //! bindings written by the config file
    std::unordered_map<std::string, std::string> kb_bindings;
    std::unordered_map<std::string, std::string> gamepad_bindings;
    std::unordered_map<std::string, std::string> joystick_bindings;

    //! default system bindings, likely not changeable
    std::unordered_map<std::string, std::string> kb_system_bindings;
    std::unordered_map<std::string, std::string> gamepad_system_bindings;
    std::unordered_map<std::string, std::string> joystick_system_bindings;

    bool disabled = false;

    //! converts the given input to a boolean value
    //! @return the converted value
    static bool binaryOf(binding_t& binding);

    //! converts the given input to a float value
    //! @return the converted value
    static float analogOf(binding_t& binding);

    //! mouse sensitivity
    static const float sensitivity;

    //! joystick deadzone
    static const float deadzone;

    //! joystick deadzone for rebinding
    static const float rebinding_deadzone;

    //! analog binding threshold
    static const float analogToggleThreshold;

    //! map of scancodes to input names
    static std::unordered_map<std::string, SDL_Keycode> keycodeNames;
    static SDL_Keycode getKeycodeFromName(const char* name);

    //! number of game ticks to consider a button 'held' for long-press actions
    static const Uint32 BUTTON_HELD_TICKS;

    //! number of game ticks to for analog button to repeatedly send
    static const Uint32 BUTTON_ANALOG_REPEAT_TICKS;
};
