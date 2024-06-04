<% @Language='JScript' %>

<OBJECT RUNAT=server ID=ServDoc PROGID="XSLISAPI.XMLServerDocument"></OBJECT>

<%
    var requestPath;
    var origPath;
    var processedFile;

    try {
        origPath = Request.ServerVariables("HTTP_SSXSLSRCFILE:");
        ServDoc.URL = origPath;
        ServDoc.UserAgent = Request.ServerVariables("HTTP_USER_AGENT:");
        requestPath = Server.MapPath(origPath);
        ServDoc.Load(requestPath);
        ServDoc.Transform(Response);
    } catch (exception) {
        ServDoc.HandleError(Response);
    }
%>

