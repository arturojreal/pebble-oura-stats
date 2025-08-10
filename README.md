# Pebble Oura Stats Watchface

A modern Pebble watchface that displays real-time health data from your Oura Ring, including sleep score, readiness score, and heart rate metrics.

This is a work in progress, and is not yet ready for production use. It is currently in beta testing!

To find out more: [The Rebble Alliance Discord](https://discord.com/channels/221364737269694464/1403471447481122897)


![Watchface Preview](https://img.shields.io/badge/Pebble-Compatible-blue) ![OAuth2](https://img.shields.io/badge/OAuth2-Secure-green) ![Status](https://img.shields.io/badge/Status-Beta-yellow)

## Features

- **Real-time Oura Data**: Sleep score, readiness score, and heart rate from Oura Ring API v2
- **Secure OAuth2**: Client-side-only authentication flow (no secrets stored)
- **Modern UI**: Large fonts, optimized layout, clean design
- **Diagnostics**: Built-in debugging and API testing tools
- **Proxy Support**: CORS-compliant API calls via Netlify Functions
- **Auto-refresh**: Hourly data updates with manual refresh option

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
5. Verify real data appears on your watch

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
    [Time - Large Font]
    
    [Debug Status]
    [Sample Indicator]
    
[Sleep] [Readiness] [Heart Rate]
 SLP      RDY         HR
```

## Configuration

### Oura Developer Setup
1. Create account at https://cloud.ouraring.com/oauth/applications
2. Create new application with these settings:
   - **Redirect URI**: `https://your-netlify-url.netlify.app/pebble-static-config.html`
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

