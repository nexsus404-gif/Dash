#import "RootViewController.h"
#import "Database.h"
#include <mach/mach_time.h>

static const CGFloat kCellHeight = 44.0f;
static const int     kPageSize   = 200;

@interface RootViewController () <UITableViewDataSource, UITableViewDelegate>
@property (nonatomic, strong) UITableView *tableView;
@property (nonatomic, strong) NSMutableArray *items; // NSArray of NSDictionary
@property (nonatomic, assign) sqlite3_int64 lastRowId;
@property (nonatomic, assign) BOOL isLoading;
@property (nonatomic, assign) BOOL hasMore;
@end

@implementation RootViewController

#pragma mark - Lifecycle

- (void)viewDidLoad {
    [super viewDidLoad];
    self.title = @"Dash";
    self.view.backgroundColor = [UIColor whiteColor];

    // Фиксированная высота = нет Auto Layout пересчёта на A8
    self.tableView = [[UITableView alloc]
        initWithFrame:self.view.bounds
        style:UITableViewStylePlain];
    self.tableView.rowHeight = kCellHeight;
    self.tableView.estimatedRowHeight = 0;       // ОТКЛЮЧИТЬ!
    self.tableView.separatorInset = UIEdgeInsetsZero;
    self.tableView.dataSource = self;
    self.tableView.delegate = self;
    [self.tableView registerClass:[UITableViewCell class] forCellReuseIdentifier:@"c"];
    self.tableView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    [self.view addSubview:self.tableView];

    self.items = [NSMutableArray arrayWithCapacity:kPageSize];
    self.hasMore = YES;

    // Замер cold-start загрузки первой страницы
    mach_timebase_info_data_t tb;
    mach_timebase_info(&tb);
    uint64_t t0 = mach_absolute_time();

    [self loadNextPage];

    uint64_t t1 = mach_absolute_time();
    double ms = (double)(t1 - t0) * tb.numer / tb.denom / 1e6;
    NSLog(@"[Dash] first page loaded in %.1f ms (%lu items)", ms, (unsigned long)self.items.count);
}

#pragma mark - Data Loading

- (void)loadNextPage {
    if (self.isLoading || !self.hasMore) return;
    self.isLoading = YES;

    __block NSMutableArray *newItems = [NSMutableArray arrayWithCapacity:kPageSize];

    // DB read is fast enough to do synchronously on main for first page.
    // For subsequent pages, move to background queue.
    int count = db_get_packages(self.lastRowId,
        ^(sqlite3_int64 rowid, const char *pkg, const char *name, const char *ver, void *ctx) {
            NSMutableDictionary *d = [NSMutableDictionary dictionary];
            d[@"rowid"]   = @(rowid);
            d[@"package"] = [NSString stringWithUTF8String:pkg ?: ""];
            d[@"name"]    = [NSString stringWithUTF8String:name ?: pkg ?: ""];
            d[@"version"] = [NSString stringWithUTF8String:ver ?: ""];
            [newItems addObject:d];
        }, NULL);

    if (count < kPageSize) self.hasMore = NO;
    if (count > 0) {
        self.lastRowId = [newItems.lastObject[@"rowid"] longLongValue];
    }

    [self.items addObjectsFromArray:newItems];
    [self.tableView reloadData];
    self.isLoading = NO;
}

#pragma mark - UITableViewDataSource

- (NSInteger)tableView:(UITableView *)tv numberOfRowsInSection:(NSInteger)s {
    return self.items.count;
}

- (UITableViewCell *)tableView:(UITableView *)tv cellForRowAtIndexPath:(NSIndexPath *)ip {
    UITableViewCell *cell = [tv dequeueReusableCellWithIdentifier:@"c" forIndexPath:ip];

    NSDictionary *item = self.items[ip.row];
    cell.textLabel.text = item[@"name"];
    cell.detailTextLabel.text = item[@"version"];

    // Prefetch next page when user scrolls near bottom
    if (ip.row >= (NSInteger)self.items.count - 20) {
        dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
            [self loadNextPage];
        });
    }

    return cell;
}

@end
