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
    const { endpoint, token, start_date, end_date } = event.queryStringParameters || {};

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
      'daily_sleep'
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

    // Build Oura API URL
    let ouraUrl = `https://api.ouraring.com/v2/usercollection/${endpoint}`;
    
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
