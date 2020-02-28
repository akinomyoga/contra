(function(agh){

  function getTextNodes(parent, dict) {
    if (dict == null) dict = [];
    var nodes = parent.childNodes;
    for (var i = 0, iN = nodes.length; i < iN; i++) {
      var node = nodes[i];
      if (node.nodeType == document.TEXT_NODE)
        dict.push(node);
      else if (node.nodeType == document.ELEMENT_NODE)
        getTextNodes(node, dict);
    }
    return dict;
  }

  agh.scripts.wait(["event:onload", "agh.dom.js"], function() {
    var dict = {};
    var idcount = 0;
    agh.Array.each(document.getElementsByTagName("dfn"), function(dfn){
      if (/(?:^|\s)(?:cfunc|cmode|sgr)(?:\s|$)/.test(dfn.className)) {
        var word = agh.dom.getInnerText(dfn);
        if (word in dict) return;
        var id;
        if (/^[a-zA-Z0-9/_]+$/.test(word)) {
          id = "dfn." + word;
        } else {
          id = "dfnx:" + idcount++;
        }
        dfn.id = id;
        dict[word] = id;
      }
    });

    agh.Array.each(getTextNodes(document.body), function(text){
      if (/^(?:dfn|a|kbd|h[1-6])$/i.test(text.parentNode.tagName)) return;

      var data = text.data;
      var result = data.replace(/[&<>]|\b[A-Za-z0-9/_]+\b/g,function($0){
        if (/[&<>]/.test($0))
          return {'&': '&amp;', '<': '&lt;', '>': '&gt'}[$0];
        else if ($0 in dict)
          return '<a class="contra-autolink" href="#' + dict[$0] + '">' + $0 + '</a>';
        else
          return $0;
      });
      if (result != data) {
        agh.dom.insert(text, result, "after");
        agh.dom.remove(text);
      }
    });

    // agh.Array.each(document.getElementsByTagName("th"), function(th) {
    //   var word = agh.dom.getInnerText(th);
    //   if (word in dict) {
    //     var id = dict[word];
    //     var a = document.createElement("a");
    //     a.className = "contra-autolink";
    //     a.href = "#" + id;
    //     agh.Array.each(agh(th.childNodes, Array), function(node) { a.appendChild(node); });
    //     th.appendChild(a);
    //   }
    // });
    
    if (window.location.hash) {
      var hash = window.location.hash;
      window.location.href = "#";
      window.location.href = hash;
    }
  });
})(window.agh);
