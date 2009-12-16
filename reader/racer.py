import conso
from conso import widgets


class BookmarksModule(widgets.ListModule):
    @widgets.action(title = "Jump", keys = ["enter"])
    def action_jump_selected(self, evt):
        i = self.get_selected_index()
        
        return True

    @widgets.action(title = "Edit", keys = ["e"])
    def action_edit_selected(self, evt):
        return True


class FiltersModule(widgets.ListModule):
    @widgets.action(title = "Toggle", keys = ["t"])
    def action_toggle_selected(self, evt):
        return True

    @widgets.action(title = "Edit", keys = ["e"])
    def action_edit_selected(self, evt):
        return True


class InputModule(widgets.FramedModule):
    def __init__(self, message, text = ""):
        self.text = TextEntry(text)
        widgets.FramedModule.__init__(self,
            VLayout(Label(message), self.text)
        )

class FunctionSearch(InputModule):
    """
    search by:
     * file name
     * module name
     * function name
    """
    def __init__(self):
        InputModule.__init__(self, "Enter function name (may be partial)")
    
    @widgets.action(title = "Forward", keys = ["enter", "f"])
    def action_search_fwd(self, evt):
        pass

    @widgets.action(title = "Backward", keys = ["shift enter", "b"])
    def action_search_fwd(self, evt):
        pass

class FunctionSearchByName(AutoWidget):
    def __init__(self):
        pass



class TimeSearch(InputModule):
    """
    search by:
     * absolute time [MM-dd] hh:mm:ss
     * relative time +/- [[hh:]mm:]ss
    """
    def __init__(self):
        InputModule.__init__(self, "Enter absolute time ([dd:]hh:mm:ss) or relative time (+/-[[hh:]mm:]ss)")
    
    @widgets.action(title = "Jump", keys = ["enter", "ctrl j"])
    def action_goto_time(self, evt):
        pass


class TraceReaderModule(widgets.FramedModule):
    def __init__(self):
        self.bookmarks_mod = BookmarksModule([widgets.TextEntry("hello%d" % i) for i in range(30)])
        self.filters_mod = FiltersModule([widgets.TextEntry("foobar%d" % i) for i in range(30)])
        widgets.FramedModule.__init__(self,
            widgets.HLayout(
                widgets.LayoutInfo(
                    widgets.Frame(
                        widgets.VListBox(
                            widgets.SimpleListModel(["trace"*20]*100),
                            allow_scroll = True,
                        ),
                        title = "Traces"
                    ),
                    priority = 200,
                ),
                widgets.BoundingBox(
                    widgets.VLayout(
                        self.bookmarks_mod,
                        self.filters_mod,
                    ),
                    max_width = 25,
                )
            )
        )

    @widgets.action(title = "Add Bookmark", keys = ["ctrl b"])
    def action_add_bookmark(self, evt):
        return True

    @widgets.action(title = "Add Filter", keys = ["ctrl f"])
    def action_add_filter(self, evt):
        return True

    @widgets.action(title = "Function Search", keys = ["ctrl s"])
    def action_search_func(self, evt):
        return True

    @widgets.action(title = "Go to Time", keys = ["ctrl t"])
    def action_goto_time(self, evt):
        return True

    @widgets.action(title = "Toggle source viewer", keys = ["ctrl r"])
    def action_show_source(self, evt):
        return True

    @widgets.action(keys = ["["])
    def action_goto_func_start(self, evt):
        return True

    @widgets.action(keys = ["]"])
    def action_goto_func_end(self, evt):
        return True
    
    @widgets.action(keys = ["alt b"])
    def action_select_bookmarks_module(self, evt):
        return True

    @widgets.action(keys = ["alt f"])
    def action_select_filters_module(self, evt):
        return True




if __name__ == "__main__":
    r = TraceReaderModule()
    app = conso.Application(r)
    app.run(exit = False)








