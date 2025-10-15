#include "main.hpp"
#include "ImageRenderer.hpp"

ImageRenderer::ImageRenderer(TFT_eSPI &display) : tft(display) {}

void ImageRenderer::begin() {
    tft.init();
    tft.setRotation(1); // Set display orientation
    
    tft.fillScreen(TFT_LOGOBACKGROUND); // Clear the display
    tft.setTextColor(TFT_WHITE, TFT_LOGOBACKGROUND  ); // Set text color and background
    currentTextSize = smallTextSize;
    tft.setTextSize(currentTextSize); // Set text size
    tft.setCursor(10, 10); // Set cursor position
    tft.println("LundaLogger Ready"); // Print a message on the display
    tft.setCursor(10, 30); // Set cursor position
    tft.println(lundaLoggerVerLbl);
    // delay(500); // Delay to allow the display to show the message
    // tft.setTextDatum(TL_DATUM);  // Top-left for manual positioning
    initPositions();
    // pushFullImage(220, 40, 100, 100, lundaLogo);
}
    
void ImageRenderer::clear() {
    tft.fillScreen(TFT_LOGOBACKGROUND);
}

void ImageRenderer::initPositions() {

    logoPos.x = 0;
    logoPos.y = 0;

    labelPos.x = 90;
    labelPos.y = 10;

    versionPos.x = 290;
    versionPos.y = 45; 
  
    servoIDPos.x = labelPos.x;
    servoIDPos.y = 60;  
  
    timePos.x = labelPos.x;
    timePos.y = 80;
  
    wiFiRectPos.x = 5;
    wiFiRectPos.y = 100; 
  
    wiFiLabelPos.x = wiFiRectPos.x + 5;
    wiFiLabelPos.y = wiFiRectPos.y - 10;
     
    wiFiAPIPPos.x = wiFiLabelPos.x;
    wiFiAPIPPos.y = wiFiLabelPos.y + 20; 

    wiFiSSIDPos.x = wiFiLabelPos.x;
    wiFiSSIDPos.y = wiFiLabelPos.y + 20 + 20;

    wiFiPromptPos.x = wiFiLabelPos.x;
    wiFiPromptPos.y = wiFiLabelPos.y + 20 + 20 + 20; 

    statusRectPos.x = 205;
    statusRectPos.y = 100;

    statusLabelPos.x = statusRectPos.x + 5 ;
    statusLabelPos.y = statusRectPos.y - 10 ;  
  
    statusCOMPos.x = 0;               // will be overridden by function
    statusCOMPos.y = 108; 
  
    statusSDPos.x = 0;                // will be overridden by function
    statusSDPos.y = 138; 

    phasePos.x = 180;
    phasePos.y = 140;
}

void ImageRenderer::drawMainScreen() {
  /// screen gfx stuff
    tft.setFreeFont(FSSB18);    
    tft.setTextColor(TFT_DEEPBLUE, TFT_LOGOBACKGROUND); // Set text color and background
    tft.setTextSize(smallTextSize); // Set text size
    tft.setCursor(10, 10); // Set cursor position
    tft.drawString("LundaLogger", 10, 10); // Print a message on the display
    tft.setCursor(10, 50); // Set cursor position for next line
    tft.setTextSize(smallTextSize); // Set text size for the next line
};

void ImageRenderer::drawLabel() {
    tft.setFreeFont(FSSB18);    
    tft.setTextColor(TFT_DEEPBLUE, TFT_LOGOBACKGROUND); // Set text color and background
    tft.setTextSize(smallTextSize); // Set text size
    // tft.setCursor(labelPos.x, labelPos.y); // Set cursor position
    tft.drawString("LundaLogger", labelPos.x, labelPos.y); // Print a message on the display
    // tft.setCursor(versionPos.x, versionPos.y); // Set cursor position for next line
    tft.setFreeFont(FSS9); 
    // tft.setTextSize(smallTextSize); // Set text size for the next line
    tft.drawString(VERSION, versionPos.x, versionPos.y, 2); // Print version on the display
}

void ImageRenderer::drawStatusField() {
    tft.setFreeFont(FSS9);   
    tft.setTextColor(TFT_DEEPBLUE, TFT_LOGOBACKGROUND);
    tft.setTextSize(1);
    tft.drawRoundRect(wiFiRectPos.x, wiFiRectPos.y, 150, 70, 10, TFT_WHITE); // White border around the screen
    tft.drawString("WiFi  ", wiFiLabelPos.x, wiFiLabelPos.y); // Print a message on the display  
}
void ImageRenderer::drawWiFiField() {
    tft.setFreeFont(FSS9);   
    tft.setTextColor(TFT_DEEPBLUE, TFT_LOGOBACKGROUND);
    tft.setTextSize(smallTextSize);
    tft.drawRoundRect(statusRectPos.x, statusRectPos.y, 150, 70, 10, TFT_WHITE); // White border around the screendelay(10000);
    tft.drawString("Status  ", statusLabelPos.x, statusLabelPos.y); // Print a message on the display  
}

void ImageRenderer::drawWiFiAPIP(String WiFiAPIP, String wiFiSSID) {
    tft.setFreeFont(FSS9);  
    tft.setTextColor(TFT_DEEPBLUE, TFT_LOGOBACKGROUND);
    tft.setTextSize(1);
    tft.drawString(WiFiAPIP, wiFiAPIPPos.x, wiFiAPIPPos.y, 2); // Print another message on the display
    tft.setTextSize(1);
    tft.drawString("SSID: " + wiFiSSID, wiFiSSIDPos.x, wiFiSSIDPos.y, 2); // Print another message on the display
}

void ImageRenderer::drawWiFiPromt(String WiFiPrompt) {
    tft.setFreeFont(FSS9);  
    tft.setTextColor(TFT_DEEPBLUE, TFT_LOGOBACKGROUND);
    tft.setTextSize(1);
    tft.drawString(WiFiPrompt,  wiFiPromptPos.x, wiFiPromptPos.y , 2); // Print another message on the display, small font
}

// void ImageRenderer::drawBreathPhase(uint8_t breathPhase) {
//     // Draw a simple representation of the breath phase
//     const int centerX = phasePos.x;
//     const int centerY = phasePos.y;
//     const int radius = 30;

//     // Clear previous phase indicator
//     tft.fillCircle(centerX, centerY, radius + 2, TFT_LOGOBACKGROUND);

//     // Draw new phase indicator
//     uint16_t color;
//     switch (breathPhase) {
//         case 0: color = TFT_BLUE; break;   // Inhale
//         case 1: color = TFT_GREEN; break;  // Hold
//         case 2: color = TFT_RED; break;    // Exhale
//         default: color = TFT_LOGOBACKGROUND; break;  // Unknown
//     }
//     tft.fillCircle(centerX, centerY, radius, color);
// } 


void ImageRenderer::drawString(const String& text, int x, int y, int font) {
    tft.drawString(text, x, y, font);
}

void ImageRenderer::drawString(const char* text, int x, int y, int font) {
      drawString(String(text), x, y, font);  // Delegate to String version
}

void ImageRenderer::drawCenteredText(const String& text, int y) {
    int x = (tft.width() - tft.textWidth(text)) / 2;
    tft.drawString(text, x, y);
}

void ImageRenderer::drawServoID(const String& servoID) {
  String prevIDText = "";
  tft.setFreeFont(FSS9);  
  tft.setTextSize(smallTextSize);

  tft.setCursor(servoIDPos.x, servoIDPos.y);
  tft.setTextColor(TFT_LOGOBACKGROUND, TFT_LOGOBACKGROUND);
  tft.print(prevIDText);

  // Draw new date/time in small font
  tft.setCursor(servoIDPos.x, servoIDPos.y);
  tft.setTextColor(TFT_DEEPBLUE, TFT_LOGOBACKGROUND);
  tft.print(servoID);
  prevIDText = servoID; // store the old text
}

void ImageRenderer::drawDateTimeAt(const DateTime& dt, int spacing) {
    static String prevDateTimeStr = "";
    
    uint16_t textColor = TFT_DEEPBLUE;
    uint16_t bgColor   = TFT_LOGOBACKGROUND;
    tft.setFreeFont(FSS9);  

    int prevFontSize = smallTextSize; // save current font size
    int smallFont = 1;                  // smaller font for date/time
    
    tft.setTextColor(textColor, bgColor);
    tft.setTextSize(smallFont);

    if (!dt.isValid()) {
      tft.setCursor(timePos.x, timePos.y);
      tft.print("Invalid Time");
 
      prevDateTimeStr = "Invalid Time";
      return;
    }

    String dateStr = String(dt.year()) + "-" +
                     String(dt.month()) + "-" +
                     String(dt.day()) + " ";

    String timeStr = String(dt.hour()) + ":" +
                     (dt.minute() < 10 ? "0" : "") + String(dt.minute()) + ":" +
                     (dt.second() < 10 ? "0" : "") + String(dt.second());

    // Erase previous date/time
    tft.setTextColor(bgColor, bgColor);
    tft.setCursor(timePos.x, timePos.y);
    tft.print(prevDateTimeStr);

    // Draw new date/time in small font
    tft.setTextColor(textColor, bgColor);
    tft.setCursor(timePos.x, timePos.y);
    tft.print(dateStr + timeStr);

    // Save for next update
    prevDateTimeStr = dateStr + timeStr;
    
    // Restore previous font size
    tft.setTextSize(prevFontSize);
}

void ImageRenderer::drawImage(int x, int y, int w, int h, const uint16_t *img) {
  tft.startWrite();
  for (int row = 0; row < h; row++) {
    for (int col = 0; col < w; col++) {
      uint16_t color = img[row * w + col];
      tft.drawPixel(x + col, y + row, color);
    }
  }
  tft.endWrite();
}

void ImageRenderer::pushFullImage(int x, int y, int w, int h, const uint16_t *img) {
  tft.pushImage(x, y, w, h, img);
}

void ImageRenderer::drawSwatch(int x, int y, int width, int height, uint16_t color, const char *label) {
  tft.fillRect(x, y, width, height, color);
  tft.setTextColor(TFT_WHITE, color); // White text on color background
  tft.setCursor(x + 5, y + (height / 2) - 8); // Vertically center the text
  tft.print(label);
}

void ImageRenderer::drawSwatch(int x, int y, uint16_t color, const char *label, bool rounded) {
  int textWidth  = tft.textWidth(label);
  int textHeight = tft.fontHeight();

  int padding = 10;
  int boxWidth  = textWidth + padding * 2;
  int boxHeight = textHeight + padding;

  if (rounded) {
    int radius = 5;
    tft.fillRoundRect(x, y, boxWidth, boxHeight, radius, color);
  } else {
    tft.fillRect(x, y, boxWidth, boxHeight, color);
  }

  tft.setTextColor(TFT_WHITE, color);
  tft.setCursor(x + padding, y + (boxHeight - textHeight) / 2);
  tft.print(label);
}


void ImageRenderer::drawSDStatusIndicator(bool isPresent) {
  const uint16_t color     = isPresent ? 0x07E0 : 0xF800;  // Green or Red
  const uint16_t backColor = isPresent ? 0x0330 : 0x4000;  // Dark Green or Dark Red
  const char* label        = isPresent ? "SD OK" : "SD FAIL";

  // 🔧 Backup current font settings
  // const GFXfont* prevFont = tft.getFreeFont();
  uint8_t        prevDatum    = tft.getTextDatum();
  uint16_t       prevFGColor  = tft.textcolor;
  uint16_t       prevBGColor  = tft.textbgcolor;

  // 🖋️ Apply swatch-specific font settings
  tft.setFreeFont(FSSB9);                    // Larger, readable font
  tft.setTextColor(color, backColor);        // Foreground & background
  tft.setTextDatum(MC_DATUM);                // Middle Center alignment

  int textWidth  = tft.textWidth("SD FAIL"); // adopt to the longest message
  int textHeight = tft.fontHeight();         // Font height
  const int paddingX = 3;
  const int paddingY = 3;

  int boxWidth  = textWidth + paddingX * 2;
  int boxHeight = textHeight + paddingY * 2;

  int x = TFT_HEIGHT - boxWidth - 10;        // Right-aligned
  int y = statusSDPos.y = 138;                                // Position under logo
  int cx = x + boxWidth / 2;
  int cy = y + boxHeight / 2;

  tft.fillRoundRect(x, y, boxWidth, boxHeight, 5, backColor);
  tft.drawString(label, cx, cy);             // Centered text

  // 🔄 Restore previous font settings
  // tft.setFreeFont(prevFont);
  tft.setTextDatum(prevDatum);
  tft.setTextColor(prevFGColor, prevBGColor);

}

void ImageRenderer::drawCOMStatusIndicator(bool isPresent) {
  const uint16_t color     = isPresent ? 0x07E0 : 0xF800;  // Green or Red
  const uint16_t backColor = isPresent ? 0x0330 : 0x4000;  // Dark Green or Dark Red
  const char* label        = isPresent ? "COM OK" : "COM FAIL";

  // 🔧 Backup current font settings
  // const GFXfont* prevFont = tft.getFreeFont();
  uint8_t        prevDatum    = tft.getTextDatum();
  uint16_t       prevFGColor  = tft.textcolor;
  uint16_t       prevBGColor  = tft.textbgcolor;

  // 🖋️ Apply swatch-specific font settings
  tft.setFreeFont(FSSB9);                    // Larger, readable font
  tft.setTextColor(color, backColor);        // Foreground & background
  tft.setTextDatum(MC_DATUM);                // Middle Center alignment

  int textWidth  = tft.textWidth("COM FAIL"); // adopt to the longest message
  int textHeight = tft.fontHeight();         // Font height
  const int paddingX = 3;
  const int paddingY = 3;

  int boxWidth  = textWidth + paddingX * 2;
  int boxHeight = textHeight + paddingY * 2;

  int x = TFT_HEIGHT - boxWidth - 10; // 10; //  // left-aligned
  int y = statusCOMPos.y; // 90;                // Position under logo
  int cx = x + boxWidth / 2;
  int cy = y + boxHeight / 2;

  tft.fillRoundRect(x, y, boxWidth, boxHeight, 5, backColor);
  tft.drawString(label, cx, cy);             // Centered text

  // 🔄 Restore previous font settings
  // tft.setFreeFont(prevFont);
  tft.setTextDatum(prevDatum);
  tft.setTextColor(prevFGColor, prevBGColor);
}

void ImageRenderer:: drawBreathPhase(uint8_t phase) {
  static String prevPhase = "";
  String phaseString = "";
  uint16_t textColor = TFT_DEEPBLUE;
  uint16_t bgColor   = TFT_LOGOBACKGROUND;
  tft.setFreeFont(FSS9);  

  int prevFontSize = smallTextSize; // save current font size
  int smallFont = 1;                  // smaller font for date/time
    
  #define inspPhase 0x10
  #define pausePhase 0x20
  #define expPhase 0x30

  //     // Draw a simple representation of the breath phase
  const int centerX = phasePos.x;
  const int centerY = phasePos.y;
  const int radius = 15;

  tft.setTextColor(textColor, bgColor);
  tft.setTextSize(smallFont);

  uint16_t color;
  switch (phase) {
    case inspPhase:
      // phaseString = "IN ";
      color = TFT_GREENISH_TINT ;   // Inhale
    break;  
    case pausePhase:
      // phaseString = " - ";
      case 1: color = TFT_SLATEBLUE ;  // Hold
    break;  
    case expPhase:
      // phaseString = "OUT";
      color = TFT_REDDISH_TINT;    // Exhale
    break;  
    default:
      // phaseString = "---";
      color = TFT_LOGOBACKGROUND;  // Unknown
    break;
  }
  tft.fillCircle(phasePos.x, phasePos.y, radius, color);
    // hostCom.println(phaseString);

    // Erase previous date/time
    // tft.setTextColor(bgColor, bgColor);
    // tft.setCursor(phasePos.x, phasePos.y);
    // tft.print(prevPhase);

    // // Draw new phase
    // tft.setTextColor(textColor, bgColor);
    // tft.setCursor(phasePos.x, phasePos.y);
    // tft.print(phaseString);
    prevPhase = phaseString;
  }
