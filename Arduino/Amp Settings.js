// ***************************************
// * Amp Settings                        *
// * R0 - Initial development            *
// * R1 - Initial release                *
// ***************************************

//Globals variables.
var ip, port, net;
var startUp = true;
var mode = 7;
var rate = 5;
var depth = 50;
var delay = 500;
var feedback = 50;
var gain = 5;
var mix = 50;
var txtSize = 24;

// Math rounding functions
( function() 
{
    /**
    * Decimal adjustment of a number.
    *
    * @param {String}  type  The type of adjustment.
    * @param {Number}  value The number.
    * @param {Integer} exp   The exponent (the 10 logarithm of the adjustment base).
    * @returns {Number} The adjusted value.
    */
    function decimalAdjust(type, value, exp) 
    {
        // If the exp is undefined or zero...
        if (typeof exp === 'undefined' || +exp === 0) 
        {
            return Math[type](value);
        }
        value = +value;
        exp = +exp;
        // If the value is not a number or the exp is not an integer...
        if (isNaN(value) || !(typeof exp === 'number' && exp % 1 === 0)) 
        {
             return NaN;
        }
        // Shift
        value = value.toString().split('e');
        value = Math[type](+(value[0] + 'e' + (value[1] ? (+value[1] - exp) : -exp)));
        // Shift back
        value = value.toString().split('e');
        return +(value[0] + 'e' + (value[1] ? (+value[1] + exp) : exp));
    }
    
    // Decimal round
    if (!Math.round10) 
    {
        Math.round10 = function(value, exp) 
        {
            return decimalAdjust('round', value, exp);
        };
    }
    
    // Decimal floor
    if (!Math.floor10) 
    {
        Math.floor10 = function(value, exp) 
        {
            return decimalAdjust('floor', value, exp);
        };
    }
    
    // Decimal ceil
    if (!Math.ceil10) 
    {
        Math.ceil10 = function(value, exp) 
        {
            return decimalAdjust('ceil', value, exp);
        };
    }
}
)();

//Called when application is started.
function OnStart()
{
    // *** ESP8266 connection details ***
    ssid = "GCA-Amp";
    pwrd = "123456789";
    ip   = "192.168.4.1";
    port = 80;
    
    // Debug client connection details
    // ip = "192.168.128.248";
    
    //Create the main graphical layout.
	CreateLayout();	

    // Check wifi is enabled.
    thisIp = app.GetIPAddress();
    if( thisIp == "0.0.0.0" ) 
    {
        app.Alert( "Please Enable Wi-Fi!","Wi-Fi Not Detected","OK" );
        setTimeout(app.Exit,5000);
    }

    app.PreventWifiSleep();
    app.PreventScreenLock( true );
    app.EnableBackKey(false);
    app.SetOrientation( "Portrait" );
    
	//Create network objects for communication with ESP8266.
	net = app.CreateNetClient( "TCP,Raw" );  
	net.SetOnConnect( net_OnConnect );

	// Will refresh settings on start only
    net.Connect( ip, port );
} // ~~~ End of OnStart() ~~~

//Create the graphical layout.
function CreateLayout()
{
	//Create a layout with objects vertically centered.
	lay = app.CreateLayout( "linear", "VCenter,FillXY" );	
  lay.SetBackground( "/Sys/Img/BlackBack.jpg" );
  app.AddLayout( lay );
    
	//Create a text label and add it to layout.
	txt = app.CreateText( "Gargasz Custom Audio" );
	txt.SetTextColor( "white" );
	txt.SetTextSize( 30 );
	txt.SetMargins( 0, 0, 0, 0.03 );
	lay.AddChild( txt );
	
	//Tremolo check box
	var text1 = "<font color=#008800>[fa-check-square-o]</font> Tremolo" ;
	txtTrem = app.CreateText( text1, 0.8, 0.04, "FontAwesome,Html" );
	txtTrem.SetOnTouchUp( OnTouchTremolo );
	txtTrem.SetTextSize( txtSize );
    txtTrem.isChecked = true; 
	lay.AddChild( txtTrem );
	
	//Create Tremolo Rate text label.
	txtRate = app.CreateText( "Rate: 5 cps" );
	txtRate.SetMargins( 0, 0.01, 0, 0 );
	lay.AddChild( txtRate );
	
	//Create Tremolo Rate seek bar.
	skbRate = app.CreateSeekBar( 0.8, -1 );
	skbRate.SetRange( 10 );
	skbRate.SetValue( 5 );
	skbRate.SetOnTouch( skbRate_OnTouch );
	lay.AddChild( skbRate );
	
	//Create Tremolo Depth text label.
	txtDepth = app.CreateText( "Depth: 50 %" );
	txtDepth.SetMargins( 0, 0.01, 0, 0 );
	lay.AddChild( txtDepth );
	
	//Create Tremolo Depth seek bar.
	skbDepth = app.CreateSeekBar( 0.8, -1 );
	skbDepth.SetRange( 100 );
	skbDepth.SetValue( 50 );
	skbDepth.SetOnTouch( skbDepth_OnTouch );
	lay.AddChild( skbDepth );
	
	//Echo check box
	var text2 = "<font color=#008800>[fa-check-square-o]</font> Echo" ;
	txtEcho = app.CreateText( text2, 0.8, 0.04, "FontAwesome,Html" );
	txtEcho.SetMargins( 0, 0.03 );
	txtEcho.SetOnTouchUp( OnTouchEcho );
	txtEcho.SetTextSize( txtSize );
    txtEcho.isChecked = true; 
	lay.AddChild( txtEcho );
	
	//Create Echo Delay text label.
	txtDelay = app.CreateText( "Delay: 500 ms" );
	txtDelay.SetMargins( 0, 0.01, 0, 0 );
	lay.AddChild( txtDelay );
	
	//Create Echo Delay seek bar.
	skbDelay = app.CreateSeekBar( 0.8, -1 );
	skbDelay.SetRange( 1000 );
	skbDelay.SetValue( 500 );
	skbDelay.SetOnTouch( skbDelay_OnTouch );
	lay.AddChild( skbDelay );
	
	//Create Echo Feedback text label.
	txtFb = app.CreateText( "Feedback: 50 %" );
	txtFb.SetMargins( 0, 0.01, 0, 0 );
	lay.AddChild( txtFb );
	
	//Create Echo Feedback seek bar.
	skbFb = app.CreateSeekBar( 0.8, -1 );
	skbFb.SetRange( 95 );
	skbFb.SetValue( 50 );
	skbFb.SetOnTouch( skbFb_OnTouch );
	lay.AddChild( skbFb );
	
	//Distort check box
	var text3 = "<font color=#008800>[fa-check-square-o]</font> Distortion" ;
	txtDist = app.CreateText( text3, 0.8, 0.04, "FontAwesome,Html" );
	txtDist.SetMargins( 0, 0.03 );
	txtDist.SetOnTouchUp( OnTouchDistort );
	txtDist.SetTextSize( txtSize );
    txtDist.isChecked = true; 
	lay.AddChild( txtDist );
	
	//Create Distortion Gain text label.
	txtGain = app.CreateText( "Gain: 5 %" );
	txtGain.SetMargins( 0, 0.01, 0, 0 );
	lay.AddChild( txtGain );
	
	//Create Distortion Gain seek bar.
	skbGain = app.CreateSeekBar( 0.8, -1 );
	skbGain.SetRange( 10 );
	skbGain.SetValue( 5 );
	skbGain.SetOnTouch( skbGain_OnTouch );
	lay.AddChild( skbGain );
	
	//Create Distortion Mix text label.
	txtMix = app.CreateText( "Mix: 50 %" );
	txtMix.SetMargins( 0, 0.01, 0, 0 );
	lay.AddChild( txtMix );
	
	//Create Distortion Mix seek bar.
	skbMix = app.CreateSeekBar( 0.8, -1 );
	skbMix.SetRange( 100 );
	skbMix.SetValue( 50 );
	skbMix.SetOnTouch( skbMix_OnTouch );
	lay.AddChild( skbMix );
	
	//Create Connect toggle button.
	btnSend = app.CreateToggle( "  Send to Amplifier  " );
	btnSend.SetOnTouch( btnSend_OnTouch );
	btnSend.SetMargins( 0, 0.02, 0, 0 );
	btnSend.SetTextColor("white");
	lay.AddChild( btnSend );
} // ~~~ End of CreateLayout() ~~~
	
//Switch checkbox icon when touched.
function OnTouchTremolo( ev )
{
    btnSend.SetTextColor("yellow");
    if( this.isChecked )
    {
        this.SetHtml( "<font color=#aa0000>[fa-square-o]</font> Tremolo" );
    }
    else
    {
        this.SetHtml( "<font color=#008800>[fa-check-square-o]</font> Tremolo " );
    }    
    this.isChecked = !this.isChecked;
    calcMode();
}

//Switch checkbox icon when touched.
function OnTouchEcho( ev )
{
    btnSend.SetTextColor("yellow");
    if( this.isChecked ) 
    {
        this.SetHtml( "<font color=#aa0000>[fa-square-o]</font> Echo" );
    }
    else
    {
        this.SetHtml( "<font color=#008800>[fa-check-square-o]</font> Echo " );
    }   
    this.isChecked = !this.isChecked;
    calcMode();
    btnSend.SetTextColor("yellow");
}

//Switch checkbox icon when touched.
function OnTouchDistort( ev )
{
    btnSend.SetTextColor("yellow");
    if( this.isChecked )
    {
        this.SetHtml( "<font color=#aa0000>[fa-square-o]</font> Distortion" );
    }
    else
    {
        this.SetHtml( "<font color=#008800>[fa-check-square-o]</font> Distortion " );
    }   
    this.isChecked = !this.isChecked;
    calcMode();
}

//Called when user touches a seek bar.
function skbRate_OnTouch( value )
{
	txtRate.SetText( "Rate: " + value + " cps" );
	rate = value;
	btnSend.SetTextColor("yellow");
}
function skbDepth_OnTouch( value )
{
	txtDepth.SetText( "Depth: " + value + " %" );
	depth = value;
	btnSend.SetTextColor("yellow");
}
function skbDelay_OnTouch( value )
{
	if( value <= 1 ) 
	{
	    value = 1;
	    skbDelay.SetValue( 1 );
	}
	txtDelay.SetText( "Delay: " + value + " ms" );
	delay = value;
	btnSend.SetTextColor("yellow");
}
function skbFb_OnTouch( value )
{
	txtFb.SetText( "Feedback: " + value + " %" );
	feedback = value;
	btnSend.SetTextColor("yellow");
}
function skbGain_OnTouch( value )
{
	txtGain.SetText( "Gain: " + value + " %" );
	gain = value;
	btnSend.SetTextColor("yellow");
}
function skbMix_OnTouch( value )
{
	txtMix.SetText( "Mix: " + value + " %" );
	mix = value;
	btnSend.SetTextColor("yellow");
}

// Calculate Mode number
function calcMode()
{
    mode = 0;
    if ( txtTrem.isChecked )
        mode = 1;
    if ( txtEcho.isChecked )
        mode = 2;
    if ( txtDist.isChecked )
        mode = 3;
    if ( txtTrem.isChecked && txtEcho.isChecked )
        mode = 4;
    if ( txtTrem.isChecked && txtDist.isChecked )
        mode = 5;
    if ( txtEcho.isChecked && txtDist.isChecked )
        mode = 6;
    if ( txtTrem.isChecked && txtEcho.isChecked && txtDist.isChecked )
        mode = 7;
}

// Calculate Mode number
function setMode()
{
    txtTrem.isChecked = false;
    txtTrem.SetHtml( "<font color=#aa0000>[fa-square-o]</font> Tremolo " );
    txtEcho.isChecked = false;
    txtEcho.SetHtml( "<font color=#aa0000>[fa-square-o]</font> Echo " );
    txtDist.isChecked = false;
    txtDist.SetHtml( "<font color=#aa0000>[fa-square-o]</font> Distortion" )
        
    if ( mode == 1 )
    {
        txtTrem.isChecked = true;
        txtTrem.SetHtml( "<font color=#008800>[fa-check-square-o]</font> Tremolo " );  
    }
    
    if ( mode == 2 )
    {
        txtEcho.isChecked = true;
        txtEcho.SetHtml( "<font color=#008800>[fa-check-square-o]</font> Echo " );
    }
    
    if ( mode == 3 )
    {
        txtDist.isChecked = true;
        txtDist.SetHtml( "<font color=#008800>[fa-check-square-o]</font> Distortion " );
    }

    if ( mode == 4 )
    {
        txtTrem.isChecked = true;
        txtTrem.SetHtml( "<font color=#008800>[fa-check-square-o]</font> Tremolo " );
        txtEcho.isChecked = true;
        txtEcho.SetHtml( "<font color=#008800>[fa-check-square-o]</font> Echo " );
    }
    
    if ( mode == 5 )
    {
        txtTrem.isChecked = true;
        txtTrem.SetHtml( "<font color=#008800>[fa-check-square-o]</font> Tremolo " );
        txtDist.isChecked = true;
        txtDist.SetHtml( "<font color=#008800>[fa-check-square-o]</font> Distortion " );
    }
    
    if ( mode == 6 )
    {
        txtEcho.isChecked = true;
        txtEcho.SetHtml( "<font color=#008800>[fa-check-square-o]</font> Echo " );
        txtDist.isChecked = true;
        txtDist.SetHtml( "<font color=#008800>[fa-check-square-o]</font> Distortion " );
    }
    
    if ( mode == 7 )
    {
        txtTrem.isChecked = true;
        txtTrem.SetHtml( "<font color=#008800>[fa-check-square-o]</font> Tremolo " );
        txtEcho.isChecked = true;
        txtEcho.SetHtml( "<font color=#008800>[fa-check-square-o]</font> Echo " );
        txtDist.isChecked = true;
        txtDist.SetHtml( "<font color=#008800>[fa-check-square-o]</font> Distortion " );
    }
}

// Send to Amp button
function btnSend_OnTouch()
{
    btnSend.SetTextColor("white");
    
    // Check wifi is present.
    thisIp = app.GetIPAddress();
    if( thisIp == "0.0.0.0" ) 
    {
        app.Alert( "Please Enable Wi-Fi!","Wi-Fi Not Detected","OK" );
        setTimeout(app.Exit,5000);
    }
    
	if( !net.IsConnected() ) 
	{
		// Connect to ESP
		net.Connect( ip, port );
	}
	else 
	{ 
		// Disconnect from ESP
		btnSend.SetChecked( false );
		net.Disconnect();
		net = "";
		net = app.CreateNetClient( "TCP,Raw" );
	    net.SetOnConnect( net_OnConnect );
	}
}

// Called by btnSend_OnTouch when connected to ESP
function net_OnConnect( connected )
{
    var req = "";
    var rate_adj = 0;
    var depth_adj = 0;
    var delay_adj = 1;
    var feedback_adj = 0;
    var gain_adj = 0;
    var mix_adj = 2;
    
	if( connected ) 
	{
	    btnSend.SetChecked( true );
	    
	    if( startUp == true)
	    {
    	    req = "GET /putsets HTTP/1.1\r\n";
    	    startUp = false;
	    }
	    else
	    {
	        // Convert SeekBar Values to amp settings
	        delay_adj = delay * 48;
            rate_adj = rate;
            gain_adj = ( gain * 0.9 ) + 2;
            feedback_adj = feedback / 100;
            depth_adj = depth / 100;
            mix_adj = mix / 500;
	        // Create HTTP request
            req = "GET /getsets/ " + mode + " " + rate_adj + " " + depth_adj + " " + delay_adj + " " + feedback_adj + " " + gain_adj + " " + mix_adj + " HTTP/1.1\r\n";
	    }

		//Send request to remote server
	    net.SendText( req, "UTF-8" );
	    
	    var msg = "", s = "";
        do msg += s = net.ReceiveText( "UTF-8" );
        while( s.length > 0 );
        
        var items = msg.split(' ');
        var itemCount = items.length;
        
        if ( itemCount == 11 )
        {
            // Parse the settings
            mode = items[4];
            delay_adj = items[5];
            rate_adj = items[6];
            gain_adj = items[7];
            feedback_adj = items[8];
            depth_adj = items[9];
            mix_adj = items[10];
            
            // Convert amp settings to SeekBar Values
            
            delay = delay_adj / 48;
            delay = Math.round10( delay, -2);
            
            rate = rate_adj;
            rate = Math.round10( rate, -2);
            
            gain = ( gain_adj - 2 ) / 0.9;
            gain = Math.round10( gain, -2);
            
            feedback = feedback_adj * 100;
            feedback = Math.round10( feedback, -2 );
            
            depth = depth_adj * 100;
            depth = Math.round10( depth, -2 );
            
            mix = mix_adj * 500;
            mix = Math.round10( mix, -2 );
            
            // Set the controls
            setMode();
            skbDelay.SetValue( delay );
            skbRate.SetValue( rate );
            skbGain.SetValue( gain );
            skbFb.SetValue( feedback );
            skbDepth.SetValue( depth );
            skbMix.SetValue( mix );
            
            // Set the labels
            txtDelay.SetText( "Delay: " + delay + " ms" );
            txtRate.SetText( "Rate: " + rate + " cps" );
            txtGain.SetText( "Gain: " + gain + " %" );
            txtFb.SetText( "Feedback: " + feedback + " %" );
            txtDepth.SetText( "Depth: " + depth + " %" );
            txtMix.SetText( "Mix: " + mix + " %" );
            
            app.ShowPopup( "Status OK", "Bottom,Short" );
        }
        else
        {
            app.ShowPopup( "ERROR reading from amp", "Bottom,Short" );
        }
      
        net.Disconnect();
        net = "";
		net = app.CreateNetClient( "TCP,Raw" );
	    net.SetOnConnect( net_OnConnect );
        btnSend.SetChecked( false );
	}
}

// Called when user touches Back button
function OnBack()
{
    var yesNo = app.CreateYesNoDialog( "Exit Amplifier Settings App?" );
    yesNo.SetOnTouch( yesNo_OnTouch );
    yesNo.Show();
}

// Exit app dialog
function yesNo_OnTouch( result )
{
    if( result=="Yes" ) app.Exit();
}