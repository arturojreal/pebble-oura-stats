# Pebble Oura Stats Watchface

A modern, feature-rich Pebble watchface that displays real-time health data from your Oura Ring, including sleep score, readiness score, and heart rate metrics with beautiful light/dark themes.

**✨ Now featuring Light/Dark Mode toggle, configurable date formats, and enhanced readability!**

To participate in beta testing, join [The Rebble Alliance Discord](https://discord.com/channels/221364737269694464/1403471447481122897)

To help support development, please consider [buying me a coffee!](https://ko-fi.com/arturojreal).

![Watchface Preview](https://img.shields.io/badge/Pebble-Compatible-blue) ![OAuth2](https://img.shields.io/badge/OAuth2-Secure-green) ![Status](https://img.shields.io/badge/Status-Ready-brightgreen) ![Themes](https://img.shields.io/badge/Themes-Light%2FDark-purple)

## ✨ Features

### 🎨 **Visual Customization**
- **Light/Dark Mode Toggle**: Switch between 🌙 dark and ☀️ light themes with complete UI color inversion
- **Enhanced Readability**: Large, bold date display with GOTHIC_24_BOLD font
- **Optimized Layout**: Perfectly positioned time and date elements for maximum visibility
- **Configurable Date Format**: Choose between MM-DD-YYYY and DD-MM-YYYY formats

### 📊 **Real-time Health Data**
- **Live Oura Ring Data**: Sleep score, readiness score, and heart rate from Oura API v2
- **Smart Data Handling**: Uses previous day's data (when Oura data is available)
- **Intelligent Caching**: Preserves valid scores, prevents displaying zeros
- **Auto-refresh**: Hourly data updates with manual refresh capability

### 🔒 **Security & Reliability**
- **Secure OAuth2**: Client-side-only authentication flow (no secrets stored)
- **CORS-Compliant**: Proxy-routed API calls via Netlify Functions
- **Token Management**: Automatic expiration handling and re-authentication
- **Error Handling**: Graceful fallbacks and comprehensive debugging tools

## Quick Start

### 1. Prerequisites
```bash
# Install uv package manager
curl -LsSf https://astral.sh/uv/install.sh | sh

# Install Pebble tools
uv tool install pebble-tool

# Install project dependencies
npm install
```

### 2. Build and Install
```bash
# Build the watchface
pebble build

# Install to emulator
pebble install --emulator basalt

# Install to device (replace with your Pebble's IP)
pebble install --phone 192.168.1.XXX
```

### 3. Configure Oura Integration
1. Open the Pebble app on your phone
2. Go to your installed watchface → Settings
3. Tap "Connect to Oura Ring"
4. Complete OAuth2 authentication
5. Configure your preferences:
   - Choose your preferred date format (MM-DD-YYYY or DD-MM-YYYY)
   - Select theme mode (🌙 Dark or ☀️ Light)
   - Adjust layout positioning if desired
6. Verify real data appears on your watch

## 🆕 What's New in Latest Version

### ✨ **Major Feature Updates**
- **🎨 Light/Dark Mode Toggle**: Complete theme system with emoji indicators
- **📅 Enhanced Date Display**: Larger, bolder font (GOTHIC_24_BOLD) for better readability
- **🔧 Improved UI Layout**: Time moved up 10px for optimal positioning
- **⚙️ Configurable Date Format**: International date format support
- **🔄 Real-time Theme Switching**: Instant color inversion without app restart

### 🐛 **Critical Bug Fixes**
- **📅 Date Calculation Bug**: Fixed UTC vs local date issue causing zero scores
- **🔗 Configuration Sync**: Enhanced settings propagation between config page and watchface
- **💾 Smart Caching**: Improved data persistence and fallback handling
- **🔐 Authentication Flow**: Streamlined OAuth2 token management

## Architecture

### Components
- **Watchface (C)**: `src/c/oura-stats-watchface.c` - Main UI and data display
- **JavaScript (ES5)**: `src/pkjs/index.js` - OAuth2, API calls, data processing
- **Config Page**: `netlify-deploy/pebble-static-config.html` - Web-based settings and diagnostics
- **Proxy**: `netlify-deploy/netlify/functions/oura-proxy.js` - CORS-compliant API gateway

### Data Flow
1. **Authentication**: OAuth2 implicit flow via config page
2. **Token Storage**: Secure localStorage with expiration tracking
3. **API Calls**: Proxy-routed requests to Oura API v2 endpoints
4. **Data Aggregation**: JavaScript collects heart rate, readiness, and sleep data
5. **Watch Display**: Flattened data sent via AppMessage to C watchface

### UI Layout
```
    [Time - Large Bold Font]
    [Date - Bold 24pt Font]
    
    [Debug Status]
    [Sample Indicator]
    
[Sleep] [Readiness] [Heart Rate]
 SLP      RDY         HR
```

**Theme Examples:**
- **🌙 Dark Mode**: White text on black background (default)
- **☀️ Light Mode**: Black text on white background

## Configuration

### 🎨 **Watchface Settings**
Access via Pebble app → Your Watchface → Settings:

- **🔗 Connect to Oura Ring**: OAuth2 authentication setup
- **📅 Date Format**: Choose MM-DD-YYYY or DD-MM-YYYY
- **🎨 Theme Mode**: Toggle between 🌙 Dark Mode and ☀️ Light Mode
- **🔧 Layout Options**: Customize data positioning (left/middle/right)
- **🐛 Debug Toggle**: Enable/disable diagnostic information

### Oura Developer Setup
1. Create account at https://cloud.ouraring.com/oauth/applications
2. Create new application with these settings:
   - **Redirect URI**: `https://peppy-pothos-093b81.netlify.app/pebble-static-config.html`
   - **Scopes**: `daily heartrate`
   - **Application Type**: Public (client-side)

### Netlify Deployment (Optional)
If you want to host your own config page:

```bash
# Install Netlify CLI
npm install -g netlify-cli

# Login to Netlify
netlify login

# Deploy from netlify-deploy directory
cd netlify-deploy
netlify deploy --prod --dir="." --functions="netlify/functions"
```

Update your Oura app redirect URI to match your deployed URL.

## Troubleshooting

### Common Issues

**"Sample" data showing instead of real data:**
- Check Settings → verify token is present and not expired
- Use "Test Oura API" in config page diagnostics
- Ensure "Use Proxy for Test" is enabled in Pebble webview
- Use "Send to Pebble" to send data to watchface

**Config page won't load:**
- Verify Netlify deployment is live
- Check browser console for errors
- Try clearing localStorage and re-authenticating

**OAuth2 errors:**
- Verify redirect URI matches exactly in Oura developer portal
- Check that required scopes (`daily heartrate`) are configured
- Ensure OAuth2 application is set to "Public" type

### Debug Tools

The config page includes comprehensive diagnostics:
- **Live Log**: Real-time debug output with copy/paste support
- **API Testing**: Direct proxy testing with status codes and response snippets
- **Token Details**: Masked token display with expiration tracking
- **Developer Status**: Environment detection and scope verification

### Log Analysis

Watch for these key debug messages:
- `[oura] JS Ready` - JavaScript initialized
- `Token found - fetching data` - Valid token detected
- `API 3/3 done` - All API calls completed
- `Sending real data!` - Data sent to watchface

## Development

### Project Structure
```
src/
├── c/                          # Watchface C code
│   └── oura-stats-watchface.c  # Main watchface implementation
├── js/                         # Configuration files
│   ├── config.js              # Pebble config integration
│   └── config.json            # AppMessage key definitions
└── pkjs/                       # Phone-side JavaScript
    └── index.js               # OAuth2, API calls, data processing

netlify-deploy/                 # Web deployment
├── pebble-static-config.html  # Main config page
├── netlify/functions/          # Serverless functions
│   └── oura-proxy.js          # CORS proxy for API calls
└── netlify.toml               # Netlify configuration
```

### Key Technologies
- **Pebble SDK 4.4**: Watchface framework
- **ES5 JavaScript**: Phone-side compatibility
- **OAuth2 Implicit Flow**: Secure client-side authentication
- **Netlify Functions**: Serverless CORS proxy
- **Oura API v2**: Health data endpoints

### Building
```bash
# Clean build
pebble clean && pebble build

# View logs (keep Pebble app in foreground)
pebble logs --phone 192.168.1.XXX

# Screenshot
pebble screenshot --phone 192.168.1.XXX
```

## Security

- **No Client Secrets**: Uses OAuth2 implicit flow (public client)
- **Token Expiration**: 30-day automatic expiration with re-auth required
- **CORS Compliance**: All API calls routed through secure proxy
- **Input Sanitization**: HTML/XSS protection in config page
- **State Validation**: CSRF protection in OAuth2 flow

## Deployment Notes

### From Previous Netlify Update:
The configuration system has been updated to use a single, canonical config page (`pebble-static-config.html`) that provides:
- ✅ Seamless OAuth2 flow without manual code entry
- ✅ Professional user experience with diagnostics
- ✅ Proxy-based API testing for Pebble webview compatibility
- ✅ Enhanced error handling and troubleshooting tools

The old callback-based system has been superseded by this static approach, which provides better reliability and user experience in the Pebble webview environment.

## Credits

**Developer**: Arturo J. Real (https://arturojreal.com)

**Framework**: Pebble SDK 4.4  
**API**: Oura Ring API v2  
**Deployment**: Netlify  

## License

This project is provided as-is for educational and personal use. Oura Ring is a trademark of Oura Health Ltd.

---

For technical support or contributions, please refer to the troubleshooting section above or examine the diagnostic tools built into the configuration page. At the time of writing, you can get in touch about this project on the [The Rebble Alliance Discord thread](https://discord.com/channels/221364737269694464/1403471447481122897)

