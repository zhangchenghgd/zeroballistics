function jsTrim(s) {return s.replace(/(^\s+)|(\s+$)/g, "");}

function TrackThisClick(strLinkArea, strLinkId, objLink)
{
    if (objLink.innerText && jsTrim(objLink.innerText))
    {
        // anchor tag, use link text
        LinkText = objLink.innerText;
    }
    else if (objLink.all && objLink.all(0).alt)
    {
        // image, use alt text
        LinkText = objLink.all(0).alt;
    }


    if (typeof(LinkText)=="undefined" || !LinkText || LinkText == "")
        LinkText = strLinkId;
         
    var strDomain = document.domain;

    ctUrl = objLink.href + "?LinkId=" + strLinkId + "&LinkArea=" + strLinkArea 
    
    if (typeof(DCSext)!="undefined") 
    {
        if (typeof(DCSext.wt_strCat)!="undefined")
            DCSext.wt_strCat=strLinkArea+"|"+detectedLocale;
        
        if (typeof(DCSext.wt_strUrl)!="undefined")
            DCSext.wt_strUrl = window.location.href.toLowerCase();
    }
        return false; 
}









var minWidth = 0;
var ideUA = false;
var dragging = false;


function windowLoaded(evt) {
    // prevent IE text selection while dragging!!!
    // Little-known trick!
    document.body.ondrag = function () { return !dragging; };
    document.body.onselectstart = function () { return !dragging; };
}


// CHange the color and image of the splitter bar
function HighlightSplitterBar(strObjName, intOnOff)
{       
    var sliderBar = document.getElementById(strObjName);
	
    if (intOnOff == 0)
    {
        sliderBar.style.backgroundImage ='url(/msdn/controls/resizablearea/en-us/lib_grippy1.gif)';
    }
    else
    {
        sliderBar.style.backgroundImage ='url(/msdn/controls/resizablearea/en-us/lib_grippy.gif)';
    }
   return;
}

//  Main 
//	All global variables written by server control
function DoControlSetup()
{
	if (browser == "Microsoft Internet Explorer")
	{
		minWidth=600;
    }
	else
	{
		minWidth=0;
    }
	this.name = "DoControlSetup";
	
   	if (browser == "Netscape")
	{
        document.addEventListener("onkeypress",KeyPressOpenCloseToc,true);
    }
    document.onkeypress = KeyPressOpenCloseToc;

    FetchResizableAreaCells();
    return;
}

//  Layout the <div> elements based on the client side renderable area
function FetchResizableAreaCells()
{
    winWidth = parseInt(document.body.clientWidth);
    oContainerCell = document.getElementById(sContainerCell);
    oNavCell = document.getElementById(sNavCell);
    oSplitterCell = document.getElementById(sSplitterCell);
    oContentCell = document.getElementById(sContentCell);

    if (GetTocPanelCookie(cookieName) != -1)
    {
        sSplitterDefault = GetTocPanelCookie(cookieName);
    }

	if (oNavCell.style.width != sSplitterDefault)
	{
	    oNavCell.style.width = sSplitterDefault;
	    oSplitterCell.style.left = sSplitterDefault;
	    oSplitterCell.style.width = "5px";
	    oContentCell.style.left = parseInt(oSplitterCell.style.width) + parseInt(sSplitterDefault) + px;
	}    

    if (winWidth - parseInt(oContentCell.style.left) < minWidth)
    {
    	var newWidth = winWidth-605;
	    if (newWidth < 0)
	    {
    		newWidth = parseInt(oNavCell.style.width);
	    }

	    oNavCell.style.width = newWidth + px;
        oSplitterCell.style.left = oNavCell.style.width;
        oSplitterCell.style.width = "5px";
        oContentCell.style.left = parseInt(oSplitterCell.style.width) + newWidth  + px;
	}
    
    ResizeContentArea();
    return;
}


//  Attach Event Handlers
function SelectSplitter()
{	// Required for Mozilla 1.0, Netscape 7.2
	if (browser == "Netscape")
	{	//	NS4: document.captureEvents(Event.MOUSEMOVE);
        oContainerCell.addEventListener("onmousemove",ResizePanel,true);
        oContainerCell.addEventListener("onmouseup",ReleaseSplitter,true);
	}
	//	IE
	//	document.onmousemove = ResizePanel;
	//	document.onmouseup = ReleaseSplitter;
	oContainerCell.onmousemove = ResizePanel;
	oContainerCell.onmouseup = ReleaseSplitter;
	dragging = true;
	return;
}

//	Changed to OnMouseMove functionality
function ResizePanel(e)
{
	var contentSizeLimit;

	if (browser == "Netscape")
	{
		sSplitterCookieX = e.pageX;
    }
	else
	{
		if (window.event.clientX - 4 < 0)
		{
		    sSplitterCookieX = 0;
		}
		else
		{
		    sSplitterCookieX = window.event.clientX - 4;
		}
	}

    //  alert("sSplitterCookieX = " + sSplitterCookieX);
    if (parseInt(sSplitterCookieX) == 0)
    {
       DisableTocPanel(true);
    }
    else
    {
       DisableTocPanel(false);       
    }  

    oNavCell.style.width = parseInt(sSplitterCookieX) + px;
    oSplitterCell.style.left = parseInt(sSplitterCookieX) + px;
    oContentCell.style.left = sSplitterCookieX + parseInt(oSplitterCell.style.width) + px;
    ResizeContentArea();
	return;
}

//	Changed to OnMouseUp functionality
function ReleaseSplitter()
{
    //  alert("sSplitterCookieX = " + sSplitterCookieX);
    if (parseInt(sSplitterCookieX) < 1)
    {
       DisableTocPanel(true);
    }
    else
    {
       DisableTocPanel(false);       
    } 
    sTmpNavCellWidth = parseInt(sSplitterCookieX) + px;
	SetTocPanelCookie(cookieName,oNavCell.style.width, days);
	oContainerCell.onmousemove = null;
	oContainerCell.onmouseup = null;
	dragging = false;
	return;
}

//	ONKEYPRESS:
//	CHECK WHICH KEYS HAVE BEEN PRESSED
//	IF A MATCH IS FOUND, SHOW/HIDE TOC PANEL
function KeyPressOpenCloseToc(e)
{
    if (browser == "Netscape")
	{	//     //   alert(e.which + "\n" + e.target.tagName);
		if (e.which == 116)
		{
		    if (e.target.tagName.toLowerCase() != "input" && e.target.tagName.toLowerCase() != "textarea" )
		    {
		        OpenClosePanel();
            }
			e.cancelBubble = true;
		}
	}
	else if(window.event.keyCode == 116)
	{   
		if (window.event.srcElement.tagName.toLowerCase() != "textarea" && window.event.srcElement.tagName.toLowerCase() != "input")
		{
		    OpenClosePanel();
		}
		window.event.cancelBubble = true;
	}
	return;
}

//  This function enables/disabled the nodes in the TOC, 
//  depending on the state/size of the TOC panel.
function DisableTocPanel(boolEnabled)
{
    var tocPanel = document.getElementById(MtpsTocCtrlTopName);
    if(tocPanel.disabled == "undefined")
    {
        tocPanel.disabled = boolEnabled;
    }
    
    if(tocPanel.style.display == "none" && boolEnabled == true)
    {
        boolEnabled == false;
    }  
   
    if (boolEnabled == true )
    {
        tocPanel.disabled = true;
        tocPanel.style.display = "none";
    }
    else
    {
        tocPanel.disabled = false;
        tocPanel.style.display = "inline";
    }
    return; 
}


//  Simple Hide/Show Functionality
//  Need to add cookie setting
//  Add CSS ClassName change for ContentPanel
function OpenClosePanel()
{
    //  alert(oNavCell.style.width + "\n" + sTmpNavCellWidth);
    if (oNavCell.style.width != "0px")
    {
        sTmpNavCellWidth = oNavCell.style.width;
        oNavCell.style.width = "0px";
        oContentCell.style.left = parseInt(oSplitterCell.style.width) + px;
        oSplitterCell.style.left = "0px";
        DisableTocPanel(true);
    }
    else if ( (oNavCell.style.width == "0px" && sTmpNavCellWidth == undefined) ||  (oNavCell.style.width == "0px" && sTmpNavCellWidth == "0px"))
    {
        oNavCell.style.width = "250px";
        oSplitterCell.style.left = "250px";
        sTmpNavCellWidth = oNavCell.style.width;
        oContentCell.style.left = parseInt(oNavCell.style.width) + parseInt(oSplitterCell.style.width) + px;
        DisableTocPanel(false); 
    }
    else
    {
        oNavCell.style.width = sTmpNavCellWidth;
        oSplitterCell.style.left = sTmpNavCellWidth;
        oContentCell.style.left = parseInt(oNavCell.style.width) + parseInt(oSplitterCell.style.width) + px;
        DisableTocPanel(false);
    }

    if (winWidth - parseInt(oContentCell.style.left) < minWidth)
    {
	    var newWidth = winWidth - minWidth - 5;
	    if (newWidth < 0)
	    {
            if (parseInt(oNavCell.style.width) > 76)
            {
		        newWidth=76;
            } 
	        else
	        {
		        newWidth = parseInt(oNavCell.style.width);
            } 
	    }
	    oNavCell.style.width = newWidth + px;
        oSplitterCell.style.left = oNavCell.style.width;
        oSplitterCell.style.width = "5px";
        oContentCell.style.left = parseInt(oSplitterCell.style.width) + newWidth  + px;
    }

    SetTocPanelCookie(cookieName,oNavCell.style.width, days);
    ResizeContentArea();
    return;
}

//  Change the size of the Content container
function ResizeContentArea()
{
    if (winWidth - parseInt(oContentCell.style.left) < minWidth)
    { 
        oContentCell.style.width = minWidth + px;
    } 
    else
    { 
	    oContentCell.style.width = winWidth - parseInt(oContentCell.style.left)  + px;
    } 
    return;
}

/*
------------------------------------------
Cookies and Misc Functions
-------------------------------------
*/
// Set cookie on each event
// Function to set the Toc panel size
// Browser must support cookies and script
function SetTocPanelCookie(sName, sValue, days)
{
    if (window.navigator.cookieEnabled == true)
    {
	    var expires = ";";
	    if (days > 0)
	    {
            var cookieDate = new Date();
		    cookieDate.setTime(cookieDate.getTime()+(days*24*360000));
            expires = "; expires=" + cookieDate.toGMTString();
	    }
        var CookieInfo = sName + "=" + escape(sValue) + expires; 
        document.cookie = CookieInfo;
   }
   return;
}

// Function to get the Toc panel size
// Browser must support cookies and script
// Special for VS, by default do not show the TOC on first instance,
// and support the cookie state on subsquent pages
function GetTocPanelCookie(sName)
{    
    var currentTocWidth = -1;
	var allCookie = String(document.cookie);
	var pos = allCookie.indexOf(sName);
    //  alert(window.history.length);
    
    if (document.referrer == "" && ideUA == true && window.history.length == 0)
	{  
	    SetTocPanelCookie(cookieName, "0px", days);
	    return currentTocWidth;
    }
    else
    {     
	    if (pos != -1 )
	    {
            var tocValue = allCookie.split("; ");

            for (i = 0 ; i < tocValue.length; i++)
            {
	            var cookieValue = tocValue[i].split("=");
	            if (sName == cookieValue[0])
	            {
		            currentTocWidth = cookieValue[1];
		            break;
	            }
            }
	    }
	}
	return currentTocWidth;
}


//	Resize the TOC Container DIV to match the Resizable Area Control
//	resize TOC Panel to match Resizable Area control
function ResizeTocPanel()
{
	var o = document.getElementById(MtpsTocCtrlTopName);
	if (typeof(o) != "undefined")
	{
	    o.style.height = parseInt(document.body.clientHeight) - parseInt(TocPanelHeight);
	    o.style.width = "100%";
	}
}

//	Scroll the current node into view
function SyncToNode()
{
	try
	{
		var o = document.getElementById(MtpsTocCtrlTopName);
				
		var oLinks = o.getElementsByTagName("A");
		for (var i = 0 ; i < oLinks.length;i++)
		{
			if (oLinks[i].href == selectedTocSyncUrl)
			{
				var oSelected = document.getElementById(oLinks[i].id);
				oSelected.focus();				
				oSelected.scrollIntoView(true);
				oSelected.parentNode.scrollIntoView(true);
			}
		}
	}
	catch(e)
	{	//alert(	e.message + "\n" + e.description);
		return false;
	}
	return true;
}





var expcalallPres = true;
var cleanedDivIds, cleanedImgIds, oExpColSpan, oExpColImg;
var expState = true;

function ExpCollAll()
{
   expcalallPres = true;
   expCollButtonToggle();  
   cleanImgVars();
   
   for (i = 0; i < cleanedDivIds.length; i++)
   {
        try
        {
            oDiv = document.getElementById(cleanedDivIds[i]);
            oImg = document.getElementById(cleanedImgIds[i]);
            
            if (expState)
            {
                oDiv.style.display = 'block';
                oImg.src = CollPath;
            }
            else
            {
                oDiv.style.display = 'none';
                oImg.src = ExpPath;
            }
        }
        catch(e)
	    {
	    }
   }
}

function expCollButtonToggle()
{
   
   if (typeof(oExpColSpan)=='undefined')
   {
        oExpColImg = document.getElementById("ExpColImg");
        oExpColSpan = document.getElementById("ExpColSpan");
   }
   
   if (expState)
   {
        oExpColImg.src = ExpPath;
        oExpColSpan.innerHTML = ExpText;
        expState = false;
   }
   else
   {
        oExpColImg.src = CollPath;
        oExpColSpan.innerHTML= CollText;
        expState = true;
   }
}

var b = window.navigator.appName;
var spr = " : ";

//	Show the languages list in the it's panel
function DDFilterOn(e)
{   
	try
	{
		//	Change Image
		ChangeDropDownImage(true);
		//	Set Position
		oMTPS_DD_PopUpDiv.style.top = oMTPS_DD_ImgArrow.height;
		
		//	Add Event Handlers
		if(oMTPS_DD_PopUpDiv.style.display != "inline")
		{
			oMTPS_DD_PopUpDiv.style.display = "inline";

            if(document.addEventListener)
            {       //  Commented for # 32609
				    //  document.addEventListener("keydown", HandleEvent, false);
				    document.addEventListener("click", TestForResizableAreaControl, false);
				    window.addEventListener("resize", TestForResizableAreaControl, false);			
            } 
            else
            { 
				    document.attachEvent("onkeydown", HandleEvent, false);
       				document.body.onclick = HideFromClick;	
            } 

		}
		else
		{
			//	oMTPS_DD_PopUpDiv.style.display = "none";
			setTimeout(HideThisMenu, 0);			
		}
		
	}
	catch(err)
	{
		alert(err.message);
	}
	e.cancelBubble = true;
	oMTPS_DD_Div.normalize();
	return;
}

// Handle the enter key for a section of a form, binding it to the provided submit buton 
function HandleEvent(event)
{ 
    var nav = window.Event ? true : false; 
    if (nav) { 
        return NetscapeEventHandler(event); 
    } else { 
        return MicrosoftEventHandler(); 
    } 
} 

function NetscapeEventHandler(e)
{ 
//trap for enter (13), escape (27) and "t" (84)
    if ((e.which == 13 || e.which == 27 || e.which == 84) && e.target.type != 'textarea' && e.target.type != 'submit')
     { 
		setTimeout(HideThisMenu, 0);
		document.removeEventListener("keydown", HandleEvent, false);
		e.cancelBubble = true;
        e.returnValue = false; 
        e.cancel = true; 
        e.preventDefault(); 
    } 
    return; 
} 

function MicrosoftEventHandler() { 
//trap for enter (13), escape (27) and "t" (84)
    if ((event.keyCode == 13 || event.keyCode == 27 || event.keyCode == 84 )&& event.srcElement.type != 'textarea' && event.srcElement.type != 'submit') { 
        event.returnValue = false; 
        event.cancel = true;
    	setTimeout(HideThisMenu, 0);
    	document.detachEvent("onkeydown", HandleEvent, false);
    	event.cancelBubble = true;
    } 
    return; 
}


//	Only used by IE
function HideFromClick(event)
{
	var objClicked = window.event.srcElement;
	var objParent = objClicked.parentNode.parentNode;
	
	if (objParent.id != oMTPS_DD_PopUpDiv.id && objParent.id != oMTPS_DD_Div.id ) 
	{
		setTimeout(HideThisMenu, 0);
		return;
	}
	else
	{
		window.event.cancelBubble = true;
		return;
	}
}

/// FF & NN only
function TestForResizableAreaControl(e)
{
	try
	{
	//	For keydown events
	if (e.type == "keydown")
	{
		//	alert(e.which);
		if (e.which == 84 || e.which == 13)
		{
			setTimeout(HideThisMenu, 0);
			document.removeEventListener("keydown", TestForResizableAreaControl, false);
			e.cancelBubble = true;
			return;
		}
	}
	else if (e.type == "resize")
	{
		setTimeout(HideThisMenu, 0);
		window.removeEventListener("resize", TestForResizableAreaControl, false);
		e.cancelBubble = true;
		return;		
	}
	
	// for mousedown event
		if (e.type == "click")
		{
			if (e.which == 1 || e.which == 32)
			{	
				var eNode = e.target;
				if (eNode.id.length == 0)
				{	//	alert("returning false");
					setTimeout(HideThisMenu, 0);
					document.removeEventListener("click", TestForResizableAreaControl, false);
					return false;
				}
				else
				{
					var pNode = document.getElementById(eNode.id).parentNode.parentNode;
					if (eNode.id != oMTPS_DD_PopUpDiv.id)
					{
						if (pNode.id != oMTPS_DD_PopUpDiv.id)
						{
							setTimeout(HideThisMenu, 0);
							document.removeEventListener("mousedown", TestForResizableAreaControl, false);
						}
					}			
					if (e.target.id == oMTPS_DD_ImgArrow.id)
					{
						setTimeout(HideThisMenu, 0);
						document.removeEventListener("mousedown", TestForResizableAreaControl, false);
					}
					if (e.target.id == oMTPS_DD_PanelLink.id)
					{
						setTimeout(HideThisMenu, 0);
						document.removeEventListener("mousedown", TestForResizableAreaControl, false);
					}
				}
				return;
			}
		}
	}
	catch(err)
	{
		alert(err.message);
	}
}

// Persist Menu long enough for client to click check boxes
function HideThisMenu()
{
	oMTPS_DD_PopUpDiv.style.display = "none";
	oMTPS_DD_ImgArrow.src = ArrowOffPath;
	document.body.onclick = null;
	return;
}

//	Handle Image changes
function ChangeDropDownImage(boolOnOff)
{
	if (boolOnOff == true)
	{
		oMTPS_DD_ImgArrow.src = ArrowOnPath;
	}
	else
	{
		oMTPS_DD_ImgArrow.src = ArrowOffPath;
	}
}


//	Set individual checkbox
function SetLangFilter(CodeSnipID)
{
	try
	{
		if (CodeSnipID != null)
		{
			ToggleLang(CodeSnipID);
		}
		
		SetFilterText();	
		SetCodeSnippetLangFilterCookie();	
	}
	catch(err)
	{
		alert(err.message);
	}
	return;
}

//	This method is only for changing the value on the client side
//	The server side code should read the cookie and set the checkbox prior to rendering 
function ToggleLang(strThis)
{
	try
	{
		var langArray = eval(strThis);
		
		for (i = 0; i < langArray.length; i++)
		{
			var thisCodeSnip = document.getElementById(langArray[i]);
			if (thisCodeSnip.className == "code")
			{
				thisCodeSnip.className = "codeHide";
			}
			else
			{
				thisCodeSnip.className = "code";
			}
		}
	}
	catch(err)
	{
		alert(err.Message);
	}
	return;
}


// set filter language test
function SetFilterText()
{
	try
	{
		var intSetoMTPS_DD_PanelLinkText = 0;
		var objLastChecked = null;
		var cbxColl = oMTPS_DD_PopUpDiv.getElementsByTagName("input");
		
		for (var i = 0; i < cbxColl.length;i++)
		{
			if (cbxColl[i].checked == true)
			{
				intSetoMTPS_DD_PanelLinkText++;
				objLastChecked = cbxColl[i].value;
			}
		}
		
		if (intSetoMTPS_DD_PanelLinkText == 0)
		{
			oMTPS_DD_PanelLink.innerHTML = strConstLangFilterText + spr + strConstLangFilterNone;
		}
		if (intSetoMTPS_DD_PanelLinkText == 1)
		{
			oMTPS_DD_PanelLink.innerHTML = strConstLangFilterText + spr + objLastChecked;
		}
		if (intSetoMTPS_DD_PanelLinkText > 1)
		{
			oMTPS_DD_PanelLink.innerHTML = strConstLangFilterText + spr + strConstLangFilterMulti;	
		}
		if (intSetoMTPS_DD_PanelLinkText == oMTPS_DD_PopUpDiv.childNodes.length)
		{
			oMTPS_DD_PanelLink.innerHTML = strConstLangFilterText + spr + strConstLangFilterAll;
		}	
		intSetoMTPS_DD_PanelLinkText = 0;
		objLastChecked = null;
	}
	catch(err)
	{
		alert(err.message);
	}
	return;
}

// Set cookie on each event
// Function to set the Toc panel size
// Browser must support cookies and script
// "days" variable declared in ResizableArea Control, line # 156
function SetCodeSnippetLangFilterCookie()
{
    var cookieDate = new Date();
	cookieDate.setTime(cookieDate.getTime()+(days*24*360000));
    expires = "expires=" + cookieDate.toGMTString();
            
	if (window.navigator.cookieEnabled == true)
	{
		var cbxColl = oMTPS_DD_PopUpDiv.getElementsByTagName("input");
	
		for (i = 0 ; i < cbxColl.length; i ++)
		{
			document.cookie = cbxColl[i].id + "_" + i + "=" + cbxColl[i].checked + "; " + expires;
		}
	}
	return;
}

//  Jscript
document.onclick = ClearThisMenu;

var oInfoCenterParent, oInfoCenterChild;

function ShowMenu(strObj, strEventObjId)
{    //  alert(strObj +"\n"+ strEventObjId);
    oInfoCenterParent =  document.getElementById(strEventObjId);
    oInfoCenterChild = document.getElementById(strObj);
    oInfoCenterChild.style.display = "inline";
    oInfoCenterChild.style.visibility = "visible";
    oInfoCenterChild.style.top = parseInt(oInfoCenterParent.offsetHeight) + parseInt(oInfoCenterParent.offsetTop);
    oInfoCenterChild.style.left = parseInt(oInfoCenterParent.offsetLeft);
}

function ShowChildMenu(strObj, strEventObjId)
{    //  alert("event : " + event.scrElement);    //   + "\nstrObj: " + strObj + "\nstrEventObjId: " + strEventObjId);
    oInfoCenterParent =  document.getElementById(strEventObjId);
    oInfoCenterChild = document.getElementById(strObj);
    oInfoCenterChild.style.display = "inline";
    oInfoCenterChild.style.visibility = "visible";
    oInfoCenterChild.style.top = parseInt(oInfoCenterParent.offsetTop);
    oInfoCenterChild.style.left = parseInt(oInfoCenterParent.offsetLeft) + parseInt(oInfoCenterParent.offsetWidth);   
    window.event.cancelBubble = true;
}

function HideChildMenu(strObj,strEventObjId)
{   
//	"MTPS_SqlCenter_MenuRoot"
    oInfoCenterParent =  document.getElementById(strEventObjId);
    oInfoCenterChild = document.getElementById(strObj);
	oInfoCenterChild.style.display = "none";
	oInfoCenterChild.style.visibility = "hidden";
	window.event.cancelBubble = true;
}

function ClearThisMenu()
{
	var obj = document.getElementById("MTPS_SqlCenter_MenuRoot");
	if (obj != null)
	{
		obj.style.display = "none";
		obj.style.visibility = "hidden";
	}
	return;
}















var cleanedDivIds, cleanedImgIds, oExpColSpan, oExpColImg;
var expState = true;

function checkExpCollAll()
{
   
   cleanImgVars();
   
   var open = false;
   var closed = false;
   
   for (i = 0; i < cleanedDivIds.length; i++)
   {
        try
        {
            oDiv = document.getElementById(cleanedDivIds[i]);
            oImg = document.getElementById(cleanedImgIds[i]);
            
            if (oDiv.style.display == 'block')
            {
                open = true;
            }
            else
            {
                closed = true;
            }
        }
        catch(e)
	    {
	    }
   }
   if (open != closed)
	{	        
	    if (open)
	        expState = false;
	    if (closed)
	        expState = true;
	        
	    if (typeof(expcalallPres)!='undefined' && expcalallPres == true)
	        expCollButtonToggle();
	}
}

function cleanImgVars()
{
   if (typeof(cleanedDivIds)=='undefined')
   {
        var r1, r2, re;
        re = /undefined/g; 
        r1 = ExpCollDivStr.replace(re, "");
        cleanedDivIds = r1.substring(0, r1.length).split(',');
        
        r2 = ExpCollImgStr.replace(re, "");
        cleanedImgIds = r2.substring(0, r2.length).split(',');
   }
}




function CopyCode(elemName)
{
    var obj = document.getElementById(elemName)
    window.clipboardData.setData("Text", obj.innerText);
}















