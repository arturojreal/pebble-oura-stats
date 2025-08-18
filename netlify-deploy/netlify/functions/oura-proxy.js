// Netlify Function to proxy Oura API requests for Pebble
// This works around Pebble JS HTTPS/CORS limitations

exports.handler = async (event, context) => {
  // Enable CORS
  const headers = {
    'Access-Control-Allow-Origin': '*',
    'Access-Control-Allow-Headers': 'Content-Type, Authorization',
    'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
    'Content-Type': 'application/json'
  };

  // Handle preflight requests
  if (event.httpMethod === 'OPTIONS') {
    return {
      statusCode: 200,
      headers,
      body: ''
    };
  }

  // Only allow GET requests
  if (event.httpMethod !== 'GET') {
    return {
      statusCode: 405,
      headers,
      body: JSON.stringify({ error: 'Method not allowed' })
    };
  }

  try {
    // Extract parameters
    const { endpoint, token: tokenFromQuery, start_date, end_date } = event.queryStringParameters || {};

    // Prefer Authorization header if present; fallback to token query param
    const authHeader = (event.headers && (event.headers.Authorization || event.headers.authorization)) || '';
    const bearerMatch = authHeader.match(/^Bearer\s+(.+)$/i);
    const token = bearerMatch ? bearerMatch[1] : tokenFromQuery;

    if (!endpoint || !token) {
      return {
        statusCode: 400,
        headers,
        body: JSON.stringify({ 
          error: 'Missing required parameters: endpoint and token' 
        })
      };
    }

    // Validate endpoint (security)
    const allowedEndpoints = [
      'heartrate',
      'daily_readiness', 
      'daily_sleep',
      'daily_activity',
      'daily_stress',
      'personal_info'
    ];

    if (!allowedEndpoints.includes(endpoint)) {
      return {
        statusCode: 400,
        headers,
        body: JSON.stringify({ 
          error: 'Invalid endpoint. Allowed: ' + allowedEndpoints.join(', ')
        })
      };
    }

    // Build Oura API URL based on endpoint type
    let ouraUrl;
    
    // Map endpoints to correct Oura API v2 paths
    const endpointMap = {
      'heartrate': 'https://api.ouraring.com/v2/usercollection/heartrate',
      'daily_readiness': 'https://api.ouraring.com/v2/usercollection/daily_readiness',
      'daily_sleep': 'https://api.ouraring.com/v2/usercollection/daily_sleep',
      'daily_activity': 'https://api.ouraring.com/v2/usercollection/daily_activity',
      'daily_stress': 'https://api.ouraring.com/v2/usercollection/daily_stress',
      'personal_info': 'https://api.ouraring.com/v2/usercollection/personal_info'
    };
    
    ouraUrl = endpointMap[endpoint];
    
    // Add date parameters if provided
    const params = new URLSearchParams();
    if (start_date) params.append('start_date', start_date);
    if (end_date) params.append('end_date', end_date);
    
    if (params.toString()) {
      ouraUrl += '?' + params.toString();
    }

    console.log('Proxying request to:', ouraUrl);

    // Make request to Oura API
    const response = await fetch(ouraUrl, {
      method: 'GET',
      headers: {
        'Authorization': `Bearer ${token}`,
        'User-Agent': 'Pebble-Oura-Stats/1.0'
      }
    });

    const data = await response.text();
    
    // Return the response
    return {
      statusCode: response.status,
      headers,
      body: data
    };

  } catch (error) {
    console.error('Proxy error:', error);
    
    return {
      statusCode: 500,
      headers,
      body: JSON.stringify({ 
        error: 'Internal server error',
        message: error.message 
      })
    };
  }
};
