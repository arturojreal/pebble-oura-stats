// Oura Stats Watchface Configuration
// Handles OAuth2 authorization and settings management

// Oura API Configuration
var OURA_CONFIG = {
  CLIENT_ID: 'TGDTXUBGWULVNKSC',
  CLIENT_SECRET: 'SJXAWZPGTKPVHTA5ZHNY4FQBGZQLWDH5',
  REDIRECT_URI: 'https://peppy-pothos-093b81.netlify.app/callback',
  AUTH_URL: 'https://cloud.ouraring.com/oauth/authorize',
  TOKEN_URL: 'https://api.ouraring.com/oauth/token',
  BASE_URL: 'https://api.ouraring.com/v2'
};

// Storage keys for configuration
var CONFIG_KEYS = {
  ACCESS_TOKEN: 'oura_access_token',
  REFRESH_TOKEN: 'oura_refresh_token',
  TOKEN_EXPIRES: 'oura_token_expires',
  REFRESH_FREQUENCY: 'refresh_frequency',
  SHOW_DEBUG: 'show_debug',
  LAST_UPDATE: 'last_update'
};

// Initialize configuration page
function init() {
  console.log('Oura Stats Config: Initializing...');
  
  // Load current settings
  loadSettings();
  
  // Check connection status
  checkConnectionStatus();
  
  // Set up event listeners
  setupEventListeners();
}

// Load settings from localStorage or URL parameters
function loadSettings() {
  // Load from Pebble app settings if available
  var settings = getQueryParam('settings');
  if (settings) {
    try {
      var parsed = JSON.parse(decodeURIComponent(settings));
      
      // Load refresh frequency
      if (parsed.refresh_frequency) {
        document.getElementById('refresh-frequency').value = parsed.refresh_frequency;
      }
      
      // Load debug setting
      if (parsed.show_debug !== undefined) {
        document.getElementById('show-debug').checked = parsed.show_debug;
      }
      
      // Load last update time
      if (parsed.last_update) {
        var lastUpdate = new Date(parseInt(parsed.last_update));
        document.getElementById('last-update').textContent = lastUpdate.toLocaleString();
      }
    } catch (error) {
      console.error('Error parsing settings:', error);
    }
  }
}

// Check if we have valid Oura tokens
function checkConnectionStatus() {
  var accessToken = localStorage.getItem(CONFIG_KEYS.ACCESS_TOKEN);
  var tokenExpires = localStorage.getItem(CONFIG_KEYS.TOKEN_EXPIRES);
  var statusElement = document.getElementById('connection-status');
  var connectItem = document.getElementById('connect-item');
  var disconnectItem = document.getElementById('disconnect-item');
  
  if (accessToken && tokenExpires) {
    var expiresAt = new Date(parseInt(tokenExpires));
    var now = new Date();
    
    if (expiresAt > now) {
      // Token is valid
      statusElement.textContent = 'Connected to Oura ✓';
      statusElement.style.color = '#00AA00';
      connectItem.style.display = 'none';
      disconnectItem.style.display = 'block';
      
      // Try to get account info
      getAccountInfo(accessToken);
    } else {
      // Token expired
      statusElement.textContent = 'Connection expired - please reconnect';
      statusElement.style.color = '#FF6600';
      connectItem.style.display = 'block';
      disconnectItem.style.display = 'none';
    }
  } else {
    // Not connected
    statusElement.textContent = 'Not connected to Oura';
    statusElement.style.color = '#666666';
    connectItem.style.display = 'block';
    disconnectItem.style.display = 'none';
  }
}

// Get Oura account information
function getAccountInfo(accessToken) {
  fetch(OURA_CONFIG.BASE_URL + '/usercollection/personal_info', {
    method: 'GET',
    headers: {
      'Authorization': 'Bearer ' + accessToken,
      'Content-Type': 'application/json'
    }
  })
  .then(function(response) {
    if (response.ok) {
      return response.json();
    }
    throw new Error('Failed to get account info');
  })
  .then(function(data) {
    if (data.email) {
      document.getElementById('account-info').textContent = data.email;
    }
  })
  .catch(function(error) {
    console.error('Error getting account info:', error);
    document.getElementById('account-info').textContent = 'Connected (email unavailable)';
  });
}

// Set up event listeners
function setupEventListeners() {
  // Handle Connect to Oura button
  document.getElementById('connect-button').addEventListener('click', function() {
    // Use client-side flow (implicit grant) to avoid CORS issues
    var authUrl = OURA_CONFIG.AUTH_URL + 
      '?client_id=' + OURA_CONFIG.CLIENT_ID +
      '&redirect_uri=' + encodeURIComponent(OURA_CONFIG.REDIRECT_URI) +
      '&response_type=token' +
      '&scope=daily';
    
    console.log('Opening OAuth2 authorization:', authUrl);
    
    // Open OAuth2 authorization in new window
    var authWindow = window.open(authUrl, 'oura_auth', 'width=500,height=600,scrollbars=yes,resizable=yes');
    
    // Listen for successful authorization from callback page
    window.addEventListener('message', function(event) {
      // Only accept messages from our own origin
      if (event.origin !== window.location.origin) return;
      
      // Received settings from callback page
      if (event.data && event.data.access_token) {
        console.log('Received OAuth2 tokens from callback');
        
        // Store the tokens
        localStorage.setItem(CONFIG_KEYS.ACCESS_TOKEN, event.data.access_token);
        if (event.data.refresh_token) {
          localStorage.setItem(CONFIG_KEYS.REFRESH_TOKEN, event.data.refresh_token);
        }
        localStorage.setItem(CONFIG_KEYS.TOKEN_EXPIRES, event.data.token_expires.toString());
        
        // Update UI
        checkConnectionStatus();
        
        // Close auth window if still open
        if (authWindow && !authWindow.closed) {
          authWindow.close();
        }
        
        // Save settings and send to watchface
        saveSettings();
        
        alert('Successfully connected to Oura!');
      }
    });
  });
  
  // Disconnect button
  document.getElementById('disconnect-button').addEventListener('click', function() {
    disconnectFromOura();
  });
  
  // Settings change handlers
  document.getElementById('refresh-frequency').addEventListener('change', function() {
    saveSettings();
  });
  
  document.getElementById('show-debug').addEventListener('change', function() {
    saveSettings();
  });
  
  // Handle OAuth callback if we're returning from authorization
  var authCode = getQueryParam('code');
  if (authCode) {
    exchangeCodeForToken(authCode);
  }
}

// Exchange authorization code for access token
function exchangeCodeForToken(code) {
  console.log('Exchanging authorization code for token...');
  
  var statusElement = document.getElementById('connection-status');
  statusElement.textContent = 'Connecting to Oura...';
  statusElement.style.color = '#0066CC';
  
  var postData = 'grant_type=authorization_code' +
    '&client_id=' + OURA_CONFIG.CLIENT_ID +
    '&client_secret=' + OURA_CONFIG.CLIENT_SECRET +
    '&redirect_uri=' + encodeURIComponent(OURA_CONFIG.REDIRECT_URI) +
    '&code=' + code;
  
  fetch(OURA_CONFIG.TOKEN_URL, {
    method: 'POST',
    headers: {
      'Content-Type': 'application/x-www-form-urlencoded'
    },
    body: postData
  })
  .then(function(response) {
    return response.json();
  })
  .then(function(data) {
    if (data.access_token) {
      // Store tokens
      localStorage.setItem(CONFIG_KEYS.ACCESS_TOKEN, data.access_token);
      if (data.refresh_token) {
        localStorage.setItem(CONFIG_KEYS.REFRESH_TOKEN, data.refresh_token);
      }
      
      // Calculate expiration time
      var expiresIn = data.expires_in || 86400; // Default to 24 hours
      var expiresAt = Date.now() + (expiresIn * 1000);
      localStorage.setItem(CONFIG_KEYS.TOKEN_EXPIRES, expiresAt.toString());
      
      console.log('OAuth2 tokens stored successfully');
      
      // Update UI
      checkConnectionStatus();
      
      // Save settings to send back to watchface
      saveSettings();
      
    } else {
      throw new Error('No access token in response');
    }
  })
  .catch(function(error) {
    console.error('Token exchange failed:', error);
    statusElement.textContent = 'Connection failed - please try again';
    statusElement.style.color = '#CC0000';
  });
}

// Disconnect from Oura
function disconnectFromOura() {
  localStorage.removeItem(CONFIG_KEYS.ACCESS_TOKEN);
  localStorage.removeItem(CONFIG_KEYS.REFRESH_TOKEN);
  localStorage.removeItem(CONFIG_KEYS.TOKEN_EXPIRES);
  
  document.getElementById('account-info').textContent = 'Not connected';
  checkConnectionStatus();
  saveSettings();
}

// Save settings and send to watchface
function saveSettings() {
  var settings = {
    access_token: localStorage.getItem(CONFIG_KEYS.ACCESS_TOKEN),
    refresh_token: localStorage.getItem(CONFIG_KEYS.REFRESH_TOKEN),
    token_expires: localStorage.getItem(CONFIG_KEYS.TOKEN_EXPIRES),
    refresh_frequency: parseInt(document.getElementById('refresh-frequency').value),
    show_debug: document.getElementById('show-debug').checked,
    last_update: localStorage.getItem(CONFIG_KEYS.LAST_UPDATE)
  };
  
  console.log('Saving settings:', settings);
  
  // Send settings back to Pebble app
  if (typeof(Pebble) !== 'undefined') {
    Pebble.sendAppMessage(settings);
    Pebble.close();
  } else {
    // For testing outside Pebble app
    console.log('Settings would be sent to Pebble:', settings);
  }
}

// Utility function to get query parameters
function getQueryParam(name) {
  var urlParams = new URLSearchParams(window.location.search);
  return urlParams.get(name);
}

// Initialize when page loads
document.addEventListener('DOMContentLoaded', init);
