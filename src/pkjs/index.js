// =============================================================================
// OURA STATS WATCHFACE - JAVASCRIPT COMPONENT (ES5 Compatible)
// =============================================================================
// Oura API configuration
// Handles OAuth2 authentication and API calls to Oura Ring API v2
// Runs on phone, sends data to Pebble watch
// =============================================================================

// OAuth2 configuration - SECURE CLIENT-SIDE-ONLY FLOW
// No client secret stored in browser - uses Oura's official client-side-only flow
var OURA_CONFIG = {
  CLIENT_ID: 'TGDTXUBGWULVNKSC',
  BASE_URL: 'https://api.ouraring.com/v2/usercollection',
  PROXY_URL: 'https://peppy-pothos-093b81.netlify.app/.netlify/functions/oura-proxy'
};

// Storage keys
var STORAGE_KEYS = {
  ACCESS_TOKEN: 'oura_access_token',
  REFRESH_TOKEN: 'oura_refresh_token',
  TOKEN_EXPIRES: 'oura_token_expires',
  LAST_UPDATE: 'oura_last_update'
};

// =============================================================================
// OAUTH2 AUTHENTICATION
// =============================================================================

function getStoredToken() {
  // Check manual setup tokens first (priority)
  var manualToken = localStorage.getItem('oura_access_token');
  
  if (manualToken) {
    console.log('Using manual setup token');
    return manualToken;
  }
  
  // Fallback to webview tokens
  var webviewToken = localStorage.getItem(STORAGE_KEYS.ACCESS_TOKEN);
  var webviewExpires = localStorage.getItem(STORAGE_KEYS.TOKEN_EXPIRES);
  
  if (webviewToken && webviewExpires && Date.now() < parseInt(webviewExpires)) {
    console.log('Using webview token');
    return webviewToken;
  }
  
  console.log('No valid token found in localStorage');
  return null;
}

function storeTokens(accessToken, refreshToken, expiresIn) {
  var expiresAt = Date.now() + (expiresIn * 1000);
  
  localStorage.setItem(STORAGE_KEYS.ACCESS_TOKEN, accessToken);
  localStorage.setItem(STORAGE_KEYS.REFRESH_TOKEN, refreshToken);
  localStorage.setItem(STORAGE_KEYS.TOKEN_EXPIRES, expiresAt.toString());
  
  console.log('Oura tokens stored successfully');
}

// SECURE CLIENT-SIDE-ONLY FLOW - No token exchange needed
// Tokens come directly from OAuth redirect via configuration page
// This eliminates the need for client secrets in the browser

function checkForConfigurationSettings() {
  console.log('Checking for OAuth2 configuration settings...');
  
  // The secure-config.html page will store tokens directly in localStorage
  // after receiving them from Oura's OAuth redirect
  var token = getStoredToken();
  
  if (token) {
    console.log('Valid token found, fetching Oura data');
    sendDebugStatus('Token found, loading data...');
    fetchAllOuraData();
  } else {
    console.log('No valid token found, user needs to configure');
    sendDebugStatus('Please configure in Pebble app');
    // Show sample data until configured
    loadSampleData();
  }
}

// CLIENT-SIDE-ONLY FLOW: No refresh tokens supported
// When tokens expire (30 days), user must re-authenticate via config page
function handleExpiredToken() {
  console.log('Token expired, user needs to re-authenticate via config page');
  sendDebugStatus('Token expired - please reconfigure');
  
  // Clear expired token
  localStorage.removeItem('oura_access_token');
  localStorage.removeItem('oura_token_expires');
  
  // Show sample data until re-configured
  loadSampleData();
}

// =============================================================================
// OURA API CALLS (Using XMLHttpRequest for ES5 compatibility)
// =============================================================================

function makeOuraRequest(endpoint, token, callback) {
  // Use proxy to work around Pebble JS HTTPS limitations
  console.log('[oura] 🔄 Making proxy request for endpoint:', endpoint);
  sendDebugStatus('Using proxy for API...');
  
  // Parse endpoint to extract the API endpoint name and parameters
  var endpointMatch = endpoint.match(/\/usercollection\/([^?]+)/);
  if (!endpointMatch) {
    console.error('❌ Invalid endpoint format:', endpoint);
    callback(new Error('Invalid endpoint format'), null);
    return;
  }
  
  var apiEndpoint = endpointMatch[1];
  
  // Extract date parameters from the endpoint (ES5-compatible)
  var query = endpoint.indexOf('?') !== -1 ? endpoint.split('?')[1] : '';
  var params = {};
  if (query) {
    var parts = query.split('&');
    for (var i = 0; i < parts.length; i++) {
      var kv = parts[i].split('=');
      var k = decodeURIComponent(kv[0] || '');
      var v = decodeURIComponent(kv.length > 1 ? kv[1] : '');
      if (k) { params[k] = v; }
    }
  }
  var startDate = params.start_date || null;
  var endDate = params.end_date || null;
  
  // Build proxy URL
  var proxyUrl = OURA_CONFIG.PROXY_URL + 
    '?endpoint=' + encodeURIComponent(apiEndpoint) +
    '&token=' + encodeURIComponent(token);
  
  if (startDate) proxyUrl += '&start_date=' + encodeURIComponent(startDate);
  if (endDate) proxyUrl += '&end_date=' + encodeURIComponent(endDate);
  
  console.log('[oura] 📡 Proxy URL:', proxyUrl.replace(token, token.substring(0, 10) + '...'));
  
  var xhr = new XMLHttpRequest();
  xhr.open('GET', proxyUrl, true);
  xhr.setRequestHeader('Content-Type', 'application/json');
  var startedAt = Date.now();
  
  xhr.onreadystatechange = function() {
    if (xhr.readyState === 4) {
      var duration = Date.now() - startedAt;
      var respLen = (xhr.responseText && xhr.responseText.length) || 0;
      console.log('[oura] 📊 Proxy response status:', xhr.status, 'endpoint:', apiEndpoint, 'timeMs:', duration, 'len:', respLen);
      sendDebugStatus('Proxy status: ' + xhr.status);
      
      if (xhr.status === 200) {
        try {
          var data = JSON.parse(xhr.responseText);
          console.log('[oura] ✅ Proxy JSON parsed. Keys:', (data && Object.keys(data)) || []);
          sendDebugStatus('Data received via proxy!');
          callback(null, data);
        } catch (error) {
          console.error('[oura] ❌ JSON parse error:', error);
          console.log('[oura] ↪︎ Raw response (first 300 chars):', (xhr.responseText || '').substring(0, 300));
          sendDebugStatus('JSON parse error');
          callback(error, null);
        }
      } else {
        console.error('[oura] ❌ Proxy error status:', xhr.status);
        console.log('[oura] ↪︎ Error body (first 300 chars):', (xhr.responseText || '').substring(0, 300));
        sendDebugStatus('Proxy error: ' + xhr.status);
        callback(new Error('Proxy error: ' + xhr.status), null);
      }
    }
  };
  
  xhr.onerror = function() {
    console.error('[oura] ❌ Proxy network error');
    sendDebugStatus('Proxy network error');
    callback(new Error('Proxy network error'), null);
  };
  
  xhr.send();
}

function fetchHeartRateData(token, callback) {
  var today = new Date().toISOString().split('T')[0];
  var endpoint = '/usercollection/heartrate?start_date=' + today + '&end_date=' + today;
  
  console.log('[oura] Fetching heart rate data for:', today);
  sendDebugStatus('Getting heart rate...');
  
  makeOuraRequest(endpoint, token, function(error, data) {
    if (error) {
      console.error('Failed to fetch heart rate data:', error);
      sendDebugStatus('HR API failed');
      callback({ data_available: false });
      return;
    }
    
    if (data && data.data && data.data.length > 0) {
      var latestData = data.data[data.data.length - 1];
      console.log('[oura] HR records:', data.data.length, 'latest keys:', Object.keys(latestData || {}));
      sendDebugStatus('HR data found!');
      callback({
        resting_heart_rate: latestData.bpm || 0,
        hrv_score: latestData.rmssd || 0,
        data_available: true
      });
    } else {
      console.log('[oura] No heart rate data available');
      sendDebugStatus('No HR data today');
      callback({ data_available: false });
    }
  });
}

function fetchReadinessData(token, callback) {
  // Fetch last 7 days to find most recent non-zero readiness score
  var endDate = new Date().toISOString().split('T')[0];
  var startDate = new Date(Date.now() - 7 * 24 * 60 * 60 * 1000).toISOString().split('T')[0];
  var endpoint = '/usercollection/daily_readiness?start_date=' + startDate + '&end_date=' + endDate;
  
  console.log('[oura] Fetching readiness data for:', today);
  sendDebugStatus('Getting readiness...');
  
  makeOuraRequest(endpoint, token, function(error, data) {
    if (error) {
      console.error('Failed to fetch readiness data:', error);
      sendDebugStatus('RDY API failed');
      callback({ data_available: false });
      return;
    }
    
    if (data && data.data && data.data.length > 0) {
      // Find most recent non-zero readiness score
      var validData = null;
      for (var i = data.data.length - 1; i >= 0; i--) {
        var record = data.data[i];
        if (record.score && record.score > 0) {
          validData = record;
          break;
        }
      }
      
      if (validData) {
        console.log('[oura] Readiness records:', data.data.length, 'using date:', validData.day, 'score:', validData.score);
        sendDebugStatus('RDY data found!');
        callback({
          readiness_score: validData.score,
          temperature_deviation: validData.temperature_deviation || 0,
          recovery_index: validData.recovery_index || 0,
          data_available: true
        });
      } else {
        console.log('[oura] No valid readiness data (all scores are 0)');
        sendDebugStatus('No valid RDY data');
        callback({ data_available: false });
      }
    } else {
      console.log('[oura] No readiness data available');
      sendDebugStatus('No RDY data today');
      callback({ data_available: false });
    }
  });
}

function fetchSleepData(token, callback) {
  // Fetch last 7 days to find most recent non-zero sleep score
  var endDate = new Date().toISOString().split('T')[0];
  var startDate = new Date(Date.now() - 7 * 24 * 60 * 60 * 1000).toISOString().split('T')[0];
  var endpoint = '/usercollection/daily_sleep?start_date=' + startDate + '&end_date=' + endDate;
  
  console.log('[oura] Fetching sleep data for:', today);
  sendDebugStatus('Getting sleep...');
  
  makeOuraRequest(endpoint, token, function(error, data) {
    if (error) {
      console.error('Failed to fetch sleep data:', error);
      sendDebugStatus('Sleep API failed');
      callback({ data_available: false });
      return;
    }
    
    if (data && data.data && data.data.length > 0) {
      // Find most recent non-zero sleep score
      var validData = null;
      for (var i = data.data.length - 1; i >= 0; i--) {
        var record = data.data[i];
        if (record.score && record.score > 0) {
          validData = record;
          break;
        }
      }
      
      if (validData) {
        console.log('[oura] Sleep records:', data.data.length, 'using date:', validData.day, 'score:', validData.score);
        sendDebugStatus('Sleep data found!');
        callback({
          sleep_score: validData.score,
          total_sleep_duration: validData.total_sleep_duration || 0,
          sleep_efficiency: validData.efficiency || 0,
          data_available: true
        });
      } else {
        console.log('[oura] No valid sleep data (all scores are 0)');
        sendDebugStatus('No valid sleep data');
        callback({ data_available: false });
      }
    } else {
      console.log('[oura] No sleep data available');
      sendDebugStatus('No sleep data today');
      callback({ data_available: false });
    }
  });
}

// =============================================================================
// DATA AGGREGATION AND WATCH COMMUNICATION
// =============================================================================

function fetchAllOuraData() {
  console.log('🚀 Starting to fetch all Oura data...');
  
  var token = getStoredToken();
  if (!token) {
    console.log('❌ No token available, loading sample data');
    sendDebugStatus('No token - using sample data');
    loadSampleData();
    return;
  }
  
  console.log('🔐 Token available:', token.substring(0, 10) + '...' + token.substring(token.length - 6));
  console.log('📊 Token length:', token.length);
  sendDebugStatus('Token found - fetching data');
  
  // Check token expiration
  var expires = localStorage.getItem('oura_token_expires');
  if (expires) {
    var isExpired = Date.now() > parseInt(expires);
    console.log('⏰ Token expires:', new Date(parseInt(expires)).toISOString());
    console.log('🔍 Token expired:', isExpired);
    
    if (isExpired) {
      console.log('❌ Token is expired, loading sample data');
      sendDebugStatus('Token expired - need reauth');
      loadSampleData();
      return;
    }
  }
  
  console.log('✅ Token valid, fetching real data');
  sendDebugStatus('Fetching from Oura API...');
  // Use the aggregator that waits for all 3 API calls, then sends to the watch
  console.log('📡 Starting aggregated API calls (3 total)');
  fetchAllOuraDataLegacy(token);
}

// Legacy function structure - keeping for compatibility
function fetchAllOuraDataLegacy(token) {
  var results = {
    heart_rate: null,
    readiness: null,
    sleep: null
  };
  
  var completed = 0;
  var total = 3;
  
  function checkComplete() {
    completed++;
    console.log('API call completed:', completed, 'of', total);
    sendDebugStatus('API ' + completed + '/' + total + ' done');
    
    if (completed > total) {
      console.error('ERROR: More completions than expected!', completed, '>', total);
      sendDebugStatus('ERROR: Too many calls!');
      return; // Prevent multiple data sends
    }
    
    if (completed === total) {
      var ouraData = {
        heart_rate: results.heart_rate || { data_available: false },
        readiness: results.readiness || { data_available: false },
        sleep: results.sleep || { data_available: false },
        last_updated: Date.now()
      };
      
      console.log('All Oura data fetched:', ouraData);
      sendDebugStatus('Sending real data!');
      localStorage.setItem(STORAGE_KEYS.LAST_UPDATE, Date.now().toString());
      sendDataToWatch(ouraData);
    }
  }
  
  fetchHeartRateData(token, function(data) {
    console.log('Heart rate callback received:', data);
    results.heart_rate = data;
    sendDebugStatus('HR callback done');
    checkComplete();
  });
  
  fetchReadinessData(token, function(data) {
    console.log('Readiness callback received:', data);
    results.readiness = data;
    sendDebugStatus('RDY callback done');
    checkComplete();
  });
  
  fetchSleepData(token, function(data) {
    console.log('Sleep callback received:', data);
    results.sleep = data;
    sendDebugStatus('Sleep callback done');
    checkComplete();
  });
}

function loadSampleData() {
  console.log('Loading sample data');
  sendDebugStatus('Using sample data');
  sendSampleDataToWatch();
}

function sendSampleDataToWatch() {
  var sampleData = {
    heart_rate: {
      resting_heart_rate: 65,
      hrv_score: 45,
      data_available: true
    },
    readiness: {
      readiness_score: 85,
      temperature_deviation: 0,
      recovery_index: 82,
      data_available: true
    },
    sleep: {
      sleep_score: 78,
      total_sleep_time: 450,
      deep_sleep_time: 90,
      data_available: true
    }
  };
  
  console.log('Sending sample Oura data to watch');
  sendDataToWatch(sampleData);
}

function sendDebugStatus(message) {
  // Only send debug messages if enabled in config
  if (!CONFIG_SETTINGS.show_debug) {
    return;
  }
  
  Pebble.sendAppMessage({
    'debug_status': message
  }, function() {
    console.log('Debug status sent:', message);
  }, function(error) {
    console.error('Failed to send debug status:', error);
  });
  
  // Auto-clear debug message after 5 minutes
  setTimeout(function() {
    Pebble.sendAppMessage({
      'debug_status': ''
    }, function() {
      console.log('Debug status cleared after 5 minutes');
    }, function(error) {
      console.error('Failed to clear debug status:', error);
    });
  }, 5 * 60 * 1000); // 5 minutes
}

function sendDataToWatch(data) {
  // Convert nested data structure to flat message keys that C code expects
  var flatData = {};
  
  // Heart rate data
  if (data.heart_rate) {
    flatData.heart_rate = 1; // Indicate heart rate data present
    flatData.resting_heart_rate = data.heart_rate.resting_heart_rate || 0;
    flatData.hrv_score = data.heart_rate.hrv_score || 0;
    flatData.data_available = data.heart_rate.data_available ? 1 : 0;
  }
  
  // Readiness data
  if (data.readiness) {
    flatData.readiness = 1; // Indicate readiness data present
    flatData.readiness_score = data.readiness.readiness_score || 0;
    flatData.temperature_deviation = data.readiness.temperature_deviation || 0;
    flatData.recovery_index = data.readiness.recovery_index || 0;
    // Note: reusing data_available key - this might cause issues, need separate keys
  }
  
  // Sleep data
  if (data.sleep) {
    flatData.sleep = 1; // Indicate sleep data present
    flatData.sleep_score = data.sleep.sleep_score || 0;
    flatData.total_sleep_time = data.sleep.total_sleep_time || 0;
    flatData.deep_sleep_time = data.sleep.deep_sleep_time || 0;
    // Note: reusing data_available key - this might cause issues, need separate keys
  }
  
  console.log('[oura] Sending flattened data to watch:', flatData);
  
  Pebble.sendAppMessage(flatData, 
    function() {
      console.log('[oura] Data sent to watch successfully');
    },
    function(error) {
      console.error('[oura] Failed to send data to watch:', error);
      try { console.log('[oura] ↪︎ Payload keys:', Object.keys(flatData)); } catch (e) {}
    }
  );
}

// =============================================================================
// PEBBLE EVENT HANDLERS
// =============================================================================

// Configuration settings (loaded from config page)
var CONFIG_SETTINGS = {
  access_token: null,
  refresh_token: null,
  token_expires: null,
  refresh_frequency: 60, // minutes
  show_debug: true
};

// Load settings from Clay configuration or localStorage
function loadConfigSettings() {
  // Check for Clay settings first (from webview config)
  var clayToken = localStorage.getItem('clay-oura_access_token');
  var clayRefresh = localStorage.getItem('clay-refresh_frequency');
  var clayDebug = localStorage.getItem('clay-show_debug');
  
  if (clayToken) {
    // Clay configuration detected
    CONFIG_SETTINGS.access_token = clayToken;
    CONFIG_SETTINGS.refresh_frequency = parseInt(clayRefresh) || 60;
    CONFIG_SETTINGS.show_debug = clayDebug === 'true';
    CONFIG_SETTINGS.token_expires = Date.now() + (30 * 24 * 60 * 60 * 1000); // 30 days
    CONFIG_SETTINGS.connected = true;
    
    console.log('Clay configuration detected - using Clay settings');
  } else {
    // Check for manual setup tokens (fallback)
    var manualToken = localStorage.getItem('oura_access_token');
    var manualExpires = localStorage.getItem('oura_token_expires');
    var manualConnected = localStorage.getItem('oura_connected');
    
    if (manualToken) {
      CONFIG_SETTINGS.access_token = manualToken;
      CONFIG_SETTINGS.token_expires = parseInt(manualExpires) || 0;
      CONFIG_SETTINGS.connected = manualConnected === 'true';
      CONFIG_SETTINGS.show_debug = false;
      CONFIG_SETTINGS.refresh_frequency = 60;
      
      console.log('Manual setup detected - using manual token');
    }
  }
  
  // Validate token expiration
  if (CONFIG_SETTINGS.access_token && CONFIG_SETTINGS.token_expires > Date.now()) {
    CONFIG_SETTINGS.connected = true;
  } else {
    CONFIG_SETTINGS.connected = false;
  }
  
  console.log('Final config state:', {
    hasToken: !!CONFIG_SETTINGS.access_token,
    connected: CONFIG_SETTINGS.connected,
    expiresAt: new Date(CONFIG_SETTINGS.token_expires).toISOString(),
    showDebug: CONFIG_SETTINGS.show_debug,
    refreshFreq: CONFIG_SETTINGS.refresh_frequency
  });
}



Pebble.addEventListener('ready', function() {
  console.log('Oura Stats Watchface JS ready - Secure Client-Side-Only Flow');
  
  // Load configuration settings
  loadConfigSettings();
  
  if (CONFIG_SETTINGS.show_debug) {
    sendDebugStatus('JS Ready');
  }
  
  // Use secure client-side-only flow - check for configuration
  checkForConfigurationSettings();
});

Pebble.addEventListener('appmessage', function(e) {
  console.log('Received message from watch:', e.payload);
  
  if (e.payload.request_data) {
    fetchAllOuraData();
  }
  
  if (e.payload.setup_auth) {
    initiateOAuth();
  }
  
  // Handle OAuth2 authorization code from watch
  if (e.payload.auth_code) {
    console.log('Received authorization code from watch');
    exchangeCodeForToken(e.payload.auth_code);
  }
});

// Show configuration page when user taps Settings
Pebble.addEventListener('showConfiguration', function() {
  var configUrl = 'https://peppy-pothos-093b81.netlify.app/pebble-static-config.html';
  console.log('Opening configuration page:', configUrl);
  Pebble.openURL(configUrl);
});

// Handle configuration settings from config page
Pebble.addEventListener('webviewclosed', function(e) {
  console.log('🔧 Configuration closed:', e.response);
  sendDebugStatus('Config page closed');
  
  if (e.response) {
    try {
      var settings = JSON.parse(decodeURIComponent(e.response));
      console.log('📥 Received config settings:', JSON.stringify(settings));
      sendDebugStatus('Settings received: ' + Object.keys(settings).join(', '));
      
      // Check if we got a token
      if (settings.oura_access_token) {
        console.log('🔐 New token received:', settings.oura_access_token.substring(0, 10) + '...');
        sendDebugStatus('New token received');
        
        // Store the token using our storage function
        localStorage.setItem('oura_access_token', settings.oura_access_token);
        localStorage.setItem('oura_token_expires', Date.now() + (30 * 24 * 60 * 60 * 1000)); // 30 days
        console.log('💾 Token stored in localStorage');
        sendDebugStatus('Token stored');
      }
      
      // Update configuration
      // ES5-safe merge (Object.assign is not available)
      for (var key in settings) {
        if (settings.hasOwnProperty(key)) {
          CONFIG_SETTINGS[key] = settings[key];
        }
      }
      
      // Store settings for persistence
      localStorage.setItem('oura_config_settings', JSON.stringify(CONFIG_SETTINGS));
      console.log('⚙️ Config settings updated and stored');
      
      // Update periodic refresh interval if changed
      updateRefreshInterval();
      
      // Check what token we have now
      var currentToken = getStoredToken();
      if (currentToken) {
        console.log('✅ Current token available:', currentToken.substring(0, 10) + '...');
        sendDebugStatus('Token available - fetching data');
        fetchAllOuraData();
      } else {
        console.log('❌ No token available after config');
        sendDebugStatus('No token available');
      }
      
    } catch (error) {
      console.error('❌ Error parsing config response:', error);
      sendDebugStatus('Config parse error: ' + error.message);
    }
  } else {
    console.log('⚠️ Configuration closed without response');
    sendDebugStatus('Config closed - no data');
  }
});

// =============================================================================
// PERIODIC DATA UPDATES
// =============================================================================

// Dynamic refresh interval (will be updated by config)
var refreshIntervalId = null;

function updateRefreshInterval() {
  // Clear existing interval
  if (refreshIntervalId) {
    clearInterval(refreshIntervalId);
  }
  
  // Set new interval based on config
  var refreshMinutes = CONFIG_SETTINGS.refresh_frequency || 60;
  var refreshMs = refreshMinutes * 60 * 1000;
  
  console.log('Setting refresh interval to', refreshMinutes, 'minutes');
  
  refreshIntervalId = setInterval(function() {
    var lastUpdate = localStorage.getItem(STORAGE_KEYS.LAST_UPDATE);
    var refreshAgo = Date.now() - refreshMs;
    
    if (!lastUpdate || parseInt(lastUpdate) < refreshAgo) {
      console.log('Periodic Oura data update (' + refreshMinutes + ' min interval)');
      fetchAllOuraData();
    }
  }, refreshMs);
}

// Initialize refresh interval
updateRefreshInterval();

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

function setPersonalAccessToken(token) {
  storeTokens(token, null, 365 * 24 * 60 * 60);
  console.log('Personal access token set');
  fetchAllOuraData();
}

console.log('Oura Stats Watchface JavaScript component loaded');
