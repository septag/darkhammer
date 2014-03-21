/***********************************************************************************
 * Copyright (c) 2012, Sepehr Taghdisian
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice, 
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation 
 *   and/or other materials provided with the distribution.
 *
 ***********************************************************************************/
 
 
(function($)    {
    function Gantt(placeholder, data_, options_)    {
        var canvasWidth = 0;
        var canvasHeight = 0;
        var canvas = null;
        var ctx = null;
        var plot = this;
        var plusImg = null;
        var minusImg = null;
        var item_h = 19;
        var prevSelected = null;
        var leftpane_rc = null;
        var rightpane_rc = null;
        var imgCnt = 2;
        var loadedImgCnt = 0;
        var unit = "ms";
        var fontname = "12px arial";
        var state = {};
        var padding = 2;
        var grid_spacing = 1;
        var leftpane_width = "25%";
        var isInit = false;
        var retryTriggered = false;
        
        plot = this;
        plot.setData = setData;
        plot.draw = draw;
        plot.resize = function() {
            updateCanvasSize();
            resizeCanvas(canvas);
        };
        
        /* options */
        if (options_.padding != null)
			padding = options_.padding;
		if (options_.grid_spacing != null)
			grid_spacing = options_.grid_spacing;
        if (options_.leftpane_width != null)
            leftpane_width = options_.leftpane_width;

        /* init */
        updateCanvasSize();
        canvasHeight = estimateHeight();
        setupLayout();
        setupCanvas();
        loadImages();
        generatePalette();
        
        /* init events */
        plusImg.onload = checkLoaded;
        minusImg.onload = checkLoaded;
        plusImg.onerror = retryLoadImg;
        minusImg.onerror = retryLoadImg;
        
        $(cvs).click(function(e) {
            var offset = $(cvs).offset();
            onClick(e.pageX - offset.left, e.pageY - offset.top);
        });
        
        function retryLoadImg()
        {
            if (!retryTriggered)    {
                plusImg.src = "plus.png";
                minusImg.src = "minus.png";
                retryTriggered = true;
            }
        }
        
        function checkLoaded()
        {
            loadedImgCnt ++;
            if (loadedImgCnt == imgCnt) {
                isInit = true;
                draw();
            }
        }
        
        function formatNumber(n)
        {
            return Math.round(n*1000) / 1000;
        }
        
        function estimateHeight()
        {
            var height = 0;
            for (var i = 0; i < data_.samples.length; i++) 
                height += item_h + padding;
            return height + 2*padding;
        }
        
        function setupLayout()
        {
            var leftpane_p = parseInt(leftpane_width)/100;
            
            leftpane_rc = {x: padding, y:padding, w: leftpane_p*canvasWidth, h: canvasHeight - 2*padding};
            rightpane_rc = {x: leftpane_rc.x + leftpane_rc.w + 2*padding, y: padding,
                w: canvasWidth - 3*padding - leftpane_rc.w, h: canvasHeight - 2*padding};
        }
        
        function calcBarOffset(t, w)
        {
            return t*w/data_.duration;
        }
        
        function generateColorForItem(item)
        {
            item.color = generateColor();
            
            if (item.childs != null)    {
                for (var i = 0; i < item.childs.length; i++)
                    generateColorForItem(item.childs[i]);
            }
        }
        
        function generatePalette()
        {
            for (var i = 0; i < data_.samples.length; i++)
                generateColorForItem(data_.samples[i]);
        }
        
        function HsvToRgb(h, s, v)
        {
            var h_i = Math.floor(h*6);
            var f = h*6 - h_i;
            var p = v * (1-s);
            var q = v * (1-f*s);
            var t = v * (1 - (1-f)*s);
            switch (h_i)    {
                case 0: return {r:v, g:t, b: p};
                case 1: return {r:q, g:v, b: p};
                case 2: return {r:p, g:v, b: t};
                case 3: return {r:p, g:q, b: v};
                case 4: return {r:t, g:p, b: v};
                case 5: return {r:v, g:p, b: q};
            }
        }
        
        function generateColor()
        {
            var golden_ratio = 0.618033988749895;
            var h = Math.random()*0xffffffff;
            h += golden_ratio;
            h %= 1;
            var clr = HsvToRgb(h, 0.5, 0.95);
            var color = "#" + Math.floor(clr.r*255).toString(16) + 
                Math.floor(clr.g*255).toString(16) + Math.floor(clr.b*255).toString(16);
            return color;
        }
        
        function ptInRect(x, y, rc)
        {
            if (x >= rc.x && x <= (rc.x + rc.w) &&
                y >= rc.y && y <= (rc.y + rc.h))
                return true;
            else
                return false;
        }
        
        function checkItemBtn(x, y, item)
        {
            if (item.btnrc != null)    {
                /* has anchor */
                if (ptInRect(x, y, item.btnrc))
                    return item;
                    
                for (var i = 0; i < item.childs.length; i++)    {
                    var childitem = checkItemBtn(x, y, item.childs[i]);
                    if (childitem != null)
                        return childitem;
                }
                    
            }
            return null;
        }
        
        function checkItemSelected(x, y, item)
        {
            if (item.itemrc != null)    {
                /* has anchor */
                if (ptInRect(x, y, item.itemrc))
                    return item;
                
                if (item.childs != null)    {    
                    for (var i = 0; i < item.childs.length; i++)    {
                        var childitem = checkItemSelected(x, y, item.childs[i]);
                        if (childitem != null)
                            return childitem;
                    }
                }
            }
            return null;            
        }
        
        function onClick(x, y)
        {
            /* check for left-pane (tree) */
            if (ptInRect(x, y, leftpane_rc))    {
                /* check againts item buttons */
                for (var i = 0; i < data_.samples.length; i++)  {
                    item = checkItemBtn(x, y, data_.samples[i]);
                    if (item != null)   {
                        if (item.opened == null)
                            item.opened = true;
                        else
                            item.opened = !item.opened;
                            
                        if (item.opened)
                            canvasHeight += item.childs.length * item_h;
                        else
                            canvasHeight -= item.childs.length * item_h;
                        resizeCanvas(cvs);
                        draw();
                    }                
                }
                
                /* check for selection */
                for (var i = 0; i < data_.samples.length; i++)  {
                    item = checkItemSelected(x, y, data_.samples[i]);
                    if (item != null)   {
                        if (prevSelected != null)
                            prevSelected.selected = false;
                        item.selected = true;
                        prevSelected = item;
                        draw();
                    }                
                }
            }
        }
        
        function loadImages() {
            plusImg = new Image();
            plusImg.src = "/js/gantt/plus.png";
            
            minusImg = new Image();
            minusImg.src = "/js/gantt/minus.png";
        }
        
        function updateCanvasSize() {
            canvasWidth = placeholder.width();
            canvasHeight = placeholder.height();
        }
        
        function createCanvas(cls) {
            var c = document.createElement("canvas");
            c.class = cls;
            c.width = canvasWidth;
            c.height = canvasHeight;
            $(c).appendTo(placeholder);
            if (!c.getContext) // excanvas hack
                c = window.G_vmlCanvasManager.initElement(c);
            c.getContext("2d").save();
            return c;
        }        
        
        function setupCanvas()  {
            placeholder.html("");
            placeholder.css({padding: 0});
            if (placeholder.css("position") == "static")
                placeholder.css("position", "relative");
            cvs = createCanvas("main-cvs");
            ctx = cvs.getContext("2d");
            
            eventHolder = $([cvs]);
        }
        
        function drawTreeItem(x, y, w, h, item, space)
        {
            var text_x = x;
            var img_y;
           
            var has_child = item.childs != null;
            if (has_child)
                text_x += plusImg.width + padding;
                
            item.itemrc = {x: x, y: y, w: w, h: h};
            
            if (item.selected)  {
                ctx.fillStyle = "#003399";
                ctx.fillRect(x, y, w , h);
                ctx.fillStyle = "#ffffff";
                ctx.fillText(item.name, text_x + padding + space, y + padding, w - 2*padding);
            }   else   {
                ctx.fillStyle = "#000000";
                ctx.fillText(item.name, text_x + padding + space, y + padding, w - 2*padding);
            }
            
            /* +/- buttons */
            if (has_child)  {
                img_y = y + h/2 - plusImg.height/2;
                item.btnrc = {x: x + padding + space, y: img_y, w: plusImg.width, h:plusImg.height};
                
                if (item.opened)
                    ctx.drawImage(minusImg, item.btnrc.x, item.btnrc.y);
                else
                    ctx.drawImage(plusImg, item.btnrc.x, item.btnrc.y);
            }
            
            /* tree lines */
            y += h;
            if (has_child && item.opened)    {
                ctx.strokeStyle = "#cfcfcf";
                ctx.lineWidth = 1;
                ctx.beginPath();
                var start_x = x + space + plusImg.width/2 + padding;

                for (var i = 0; i < item.childs.length; i++)    {
                    var line_y = y + h/2;
                    ctx.moveTo(start_x, line_y);
                    ctx.lineTo(start_x + 10, line_y);
                    if (i == item.childs.length - 1)    {
                        ctx.moveTo(start_x, img_y + plusImg.height + padding);
                        ctx.lineTo(start_x, line_y);
                    }
                    ctx.stroke();
                    
                    y = drawTreeItem(x, y, w, h, item.childs[i], space + 20);
                }
            }

            return y;
        }
        
        function drawTree(items)
        {
            var item_x = leftpane_rc.x;
            var item_w = leftpane_rc.w + padding;
            var item_y = leftpane_rc.y;

            ctx.font = fontname;
            ctx.textBaseline = "top";
                        
            for (var i = 0; i < items.length; i++)  { 
                item_y = drawTreeItem(item_x, item_y, item_w, item_h, items[i], 0);
            }
        }
        
        function drawTimeBar(x, y, w, h, item)
        {
            /* the bar itself */            
            var bar_x = calcBarOffset(item.start, w);
            var bar_w = calcBarOffset(item.start + item.duration, w) - bar_x;
            
            ctx.fillStyle = item.color;
            ctx.fillRect(x + bar_x, y, bar_w, item_h); 
            
            /* text inside */
            ctx.fillStyle = "#000000";            
            var text = formatNumber(item.duration).toString() + " " + unit;
            var text_x = x + bar_x + bar_w/2 - ctx.measureText(text).width/2;
            if (text_x < (x + bar_x))
                text_x = x + bar_x;
            ctx.fillText(text, text_x, y + 2);

            /* selection */   
            if (item.selected)  {
                ctx.strokeStyle = "#003399";
                ctx.lineWidth = 1;
                ctx.beginPath();
                ctx.moveTo(x - padding, y + item_h - 1);
                ctx.lineTo(x + w + padding, y + item_h - 1);
                ctx.stroke();
            }            

            y += item_h;
            
            if (item.childs != null && item.opened) {
                for (var i = 0; i < item.childs.length; i++)
                    y = drawTimeBar(x, y, w, h, item.childs[i]);
            }
            return y;
        }
        
        function drawGrid()
        {
            var duration = data_.duration;
            var d = calcBarOffset(grid_spacing, rightpane_rc.w);
            var cnt = Math.ceil(rightpane_rc.w / d);
            
            ctx.strokeStyle = "#e9e9e9"
            ctx.lineWidth = 1;
            
            for (var i = 1; i < cnt; i++)   {
                var x = i*d;
                ctx.beginPath();
                ctx.moveTo(x + rightpane_rc.x, rightpane_rc.y);
                ctx.lineTo(x + rightpane_rc.x, rightpane_rc.y + rightpane_rc.h);
                ctx.stroke();
            }
        }
        
        function drawTimeline(items)
        {
            ctx.textBaseLine = "top";
            ctx.font = "bold " + fontname;

            var item_x = rightpane_rc.x + padding;
            var item_w = rightpane_rc.w - 2*padding;
            var item_y = rightpane_rc.y;
                        
            for (var i = 0; i < items.length; i++) 
                item_y = drawTimeBar(item_x, item_y, item_w, item_h, items[i]);
        }
        
        function draw() {
            if (!isInit)
                return;
                
            ctx.lineWidth = 2;
            ctx.strokeStyle = "#999999";
            ctx.fillStyle = "whitesmoke";
           
            /* left pane bg (hierarchy) */
            ctx.fillRect(leftpane_rc.x, leftpane_rc.y, leftpane_rc.x + leftpane_rc.w, 
                leftpane_rc.y + leftpane_rc.h);
            
            /* right pane bg (timeline) */
            ctx.fillRect(rightpane_rc.x, rightpane_rc.y, rightpane_rc.x + rightpane_rc.w, 
                rightpane_rc.y + rightpane_rc.h);
			
			ctx.beginPath();
			ctx.moveTo(leftpane_rc.x + leftpane_rc.w + 1, leftpane_rc.y + leftpane_rc.h);
			ctx.lineTo(leftpane_rc.x + leftpane_rc.w+ 1, leftpane_rc.y);  
			ctx.stroke();	           
                
            /* timeline grid */
            drawGrid();
            
            /* tree hierarchy (left pane) */
            drawTree(data_.samples, 0);
            
            /* timeline bars (right pane) */
            drawTimeline(data_.samples);
        }

        function loadItemState(item)
        {
            s = state[item.name];
            if (s != null)  {
                item.selected = s.selected;
                item.opened = s.opened;
                item.color = s.color;
                if (item.selected)
                    prevSelected = item;
            }
            
            if (item.childs != null)    {
            for (var i = 0; i < item.childs.length; i++)
                loadItemState(item.childs[i]);
            }
        }
        
        function saveItemState(item)
        {
            state[item.name] = {color: item.color, selected: item.selected, opened: item.opened};
            
            if (item.childs != null)    {
                for (var i = 0; i < item.childs.length; i++)
                    saveItemState(item.childs[i]);
            }
        }
        
        function loadState()
        {
            for (var i = 0; i < data_.samples.length; i++)
                loadItemState(data_.samples[i]);
        }
        
        function saveState()
        {
            prevSelected = null;
            for (var i = 0; i < data_.samples.length; i++)
                saveItemState(data_.samples[i]);
        }
        
        function resizeCanvas(c)    {
            if (c.width != canvasWidth)
                c.width = canvasWidth;
            if (c.height != canvasHeight)
                c.height = canvasHeight;
            var cctx = c.getContext("2d");
            cctx.restore();
            cctx.save();
            setupLayout();
        }
        
        function setData(newdata)  {
            if (isInit) {
                saveState();
                delete data_;
                data_ = newdata;
                loadState();
                draw();
            }
        }  
    }
    
    $.gantt = function(placeholder, data, options)  {
        var gantt = new Gantt($(placeholder), data, options);
        return gantt;
    };
    
    $.gantt.version = "0.1";
    
})(jQuery);
